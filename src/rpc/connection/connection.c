/**
 *    Copyright (C) 2015 splone UG
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    This file incorporates code covered by the following terms:
 *
 *    Copyright Neovim contributors. All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <msgpack/object.h>
#include <msgpack/pack.h>
#include <msgpack/sbuffer.h>
#include <msgpack/unpack.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>
#include <uv.h>

#include "rpc/sb-rpc.h"
#include "api/sb-api.h"
#include "sb-common.h"

static void parse_cb(inputstream *istream, void *data, bool eof);
static void close_cb(uv_handle_t *handle);
static int connection_handle_request(struct connection *con,
    msgpack_object *obj);
static int connection_handle_response(struct connection *con,
    msgpack_object *obj);
static void connection_request_event(connection_request_event_info *info);
static void connection_close(struct connection *con);

static struct hashmap_string *connections = NULL;
static msgpack_sbuffer sbuf;
equeue *equeue_root;
uv_loop_t loop;

int connection_init(void)
{
  connections = hashmap_string_new();

  if (dispatch_table_init() == -1)
    return (-1);

  if (!connections)
    return (-1);

  msgpack_sbuffer_init(&sbuf);

  return (0);
}

int connection_teardown(void)
{
  if (!connections)
    return (-1);

  struct connection *con;

  HASHMAP_ITERATE_VALUE(connections, con, {
    connection_close(con);
    FREE(con);
  });

  hashmap_string_free(connections);
  dispatch_teardown();
  msgpack_sbuffer_destroy(&sbuf);

  return (0);
}

int connection_create(uv_stream_t *stream)
{
  stream->data = NULL;

  struct connection *con = MALLOC(struct connection);

  if (con == NULL)
    return (-1);

  con->msgid = 1;
  con->mpac = msgpack_unpacker_new(MSGPACK_UNPACKER_INIT_BUFFER_SIZE);
  con->closed = false;
  con->queue = equeue_new(equeue_root);
  con->streams.read = inputstream_new(parse_cb, 1024, con);
  con->streams.write = outputstream_new(1024 * 1024);
  con->streams.uv = stream;

  kv_init(con->callvector);

  inputstream_set(con->streams.read, stream);
  inputstream_start(con->streams.read);
  outputstream_set(con->streams.write, stream);

  return (0);
}

static void connection_close(struct connection *con)
{
  int is_closing;
  uv_handle_t *handle;

  if (con->closed)
    return;

  inputstream_free(con->streams.read);
  outputstream_free(con->streams.write);
  handle = (uv_handle_t *)con->streams.uv;

  is_closing = uv_is_closing(handle);

  if (handle && !is_closing)
    uv_close(handle, close_cb);

  kv_destroy(con->callvector);

  con->closed = 0;
}


static void close_cb(uv_handle_t *handle)
{
  FREE(handle->data);
  FREE(handle);
}


static void parse_cb(inputstream *istream, void *data, bool eof)
{
  struct connection *con = data;
  size_t size;
  msgpack_unpacked result;

  if (eof) {
    connection_close(con);
    return;
  }

  size = inputstream_pending(istream);

  /* reserve space for internal msgpack buffer */
  msgpack_unpacker_reserve_buffer(con->mpac, size);
  /* fill internal msgpack buffer by size byte */
  inputstream_read(istream, msgpack_unpacker_buffer(con->mpac), size);
  /* notify deserializer that the internal mspack buffer is filled */
  msgpack_unpacker_buffer_consumed(con->mpac, size);

  /* initialize msgpack object */
  msgpack_unpacked_init(&result);
  msgpack_unpack_return ret;

  /* deserialize objects, one by one */
  while ((ret =
      msgpack_unpacker_next(con->mpac, &result)) == MSGPACK_UNPACK_SUCCESS) {
    if (message_is_request(&result.data))
      connection_handle_request(con, &result.data);
    else if (message_is_response(&result.data)) {
      if (connection_handle_response(con, &result.data) != 0) {
        break;
      }
    } else {
      /* invalid message, send response with error */
    }
  }
}

int connection_hashmap_put(string pluginlongtermpk, struct connection *con)
{
  hashmap_string_put(connections, pluginlongtermpk, con);

  return (0);
}

static int connection_write(struct connection *con)
{
  char *data;

  data = MALLOC_ARRAY(sbuf.size, char);

  if (data == NULL)
    return (-1);

  if (outputstream_write(con->streams.write, sbuf.data, sbuf.size) < 0)
    return (-1);

  FREE(data);

  return (0);
}

struct callinfo * connection_send_request(string pluginlongtermpk, string method,
    struct message_params_object *params, struct api_error *api_error)
{
  struct connection *con;
  msgpack_packer packer;
  struct message_request request;
  struct callinfo *cinfo;

  con = hashmap_string_get(connections, pluginlongtermpk);

  /*
   * if no connection is available for the key, set the connection to the
   * the initial connection from the sender.
   */
  if (!con) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "plugin not registered");
    return (NULL);
  }

  request.type = MESSAGE_TYPE_REQUEST;
  request.msgid = con->msgid++;
  request.method = method;
  request.params = *params;

  msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);

  message_serialize_request(&request, &packer);
  /* if error is set, generate an error response message */
  if (api_error->isset)
    return (NULL);

  if (connection_write(con) < 0)
    return (NULL);

  cinfo = loop_wait_for_response(con, &request);

  msgpack_sbuffer_clear(&sbuf);

  return cinfo;
}

int connection_send_response(struct connection *con, uint32_t msgid,
    struct message_params_object *params, struct api_error *api_error)
{
  msgpack_packer packer;
  struct message_response response;

  /*
   * if no connection is available for the key, set the connection to the
   * the initial connection from the sender.
   */
  if (!con) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "plugin not registered");
    return (-1);
  }

  response.msgid = msgid;
  response.params = *params;

  msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);
  message_serialize_response(&response, &packer);

  if (api_error->isset)
    return (-1);

  if (connection_write(con) < 0)
    return (-1);

  msgpack_sbuffer_clear(&sbuf);

  return 0;
}


static int connection_handle_request(struct connection *con,
    msgpack_object *obj)
{
  struct dispatch_info *dispatcher = NULL;
  struct api_error api_error = { .isset = false };
  connection_request_event_info eventinfo;
  api_event event;

  if (!obj || !con)
    return (-1);

  eventinfo.request = message_deserialize_request(obj, &api_error);

  /* request wasn't parsed correctly, send error with pseudo RESPONSE ID*/
  if (!eventinfo.request) {
    eventinfo.request = CALLOC(1, struct message_request);
    eventinfo.request->msgid = MESSAGE_RESPONSE_UNKNOWN;
  } else {
    dispatcher = dispatch_table_get(eventinfo.request->method);
  }

  if (!dispatcher) {
    error_set(&api_error, API_ERROR_TYPE_VALIDATION, "could not dispatch method");
    dispatcher = MALLOC(struct dispatch_info);
    dispatcher->func = handle_error;
    dispatcher->async = true;
  }

  LOG_VERBOSE(VERBOSE_LEVEL_0, "received request: method = %s\n",
      eventinfo.request->method.str);

  eventinfo.con = con;
  eventinfo.api_error = api_error;
  eventinfo.dispatcher = dispatcher;

  if (dispatcher->async)
    connection_request_event(&eventinfo);
  else {
    event.handler = connection_request_event;
    event.info = eventinfo;
    equeue_put(con->queue, event);
    /* TODO: move this call to a suitable place (main?) */
    equeue_run_events(equeue_root);
  }

  return (0);
}


static void connection_request_event(connection_request_event_info *eventinfo)
{
  char *data;
  msgpack_packer packer;

  eventinfo->dispatcher->func(eventinfo);

  if (eventinfo->api_error.isset) {
    msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);
    message_serialize_error_response(&packer, &eventinfo->api_error, eventinfo->request->msgid);
    data = MALLOC_ARRAY(sbuf.size, char);

    if (data == NULL)
      return;

    outputstream_write(eventinfo->con->streams.write, memcpy(data, sbuf.data,
        sbuf.size), sbuf.size);

    msgpack_sbuffer_clear(&sbuf);
    FREE(data);
  }

  FREE(eventinfo->request);
}


static int connection_handle_response(struct connection *con,
    msgpack_object *obj)
{
  struct callinfo *cinfo;
  size_t csize;
  size_t i;
  struct api_error api_error = { .isset = false };

  message_is_error_response(obj);

  csize = kv_size(con->callvector);
  cinfo = kv_A(con->callvector, csize - 1);

  if (cinfo->msgid != message_get_id(obj)) {
    for (i = 0; i < csize; i++) {
      cinfo = kv_A(con->callvector, i);
      cinfo->errorresponse = true;
      cinfo->response = NULL;
      cinfo->hasresponse = true;
    }

    connection_close(con);

    return (-1);
  }

  cinfo->errorresponse = message_is_error_response(obj);

  if (cinfo->errorresponse) {
    cinfo->response = message_deserialize_error_response(obj, &api_error);
  } else {
    cinfo->response = message_deserialize_response(obj, &api_error);
  }

  /* unblock */
  cinfo->hasresponse = true;

  return (0);
}
