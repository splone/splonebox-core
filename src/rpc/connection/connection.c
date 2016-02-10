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
#include <uv.h>

#include "rpc/sb-rpc.h"
#include "api/sb-api.h"
#include "sb-common.h"

static struct hashmap_string *connections = NULL;
static uint64_t id_cnt = 1;
static void parse_cb(inputstream *istream, void *data, bool eof);
static void close_cb(uv_handle_t *handle);
static int connection_handle_request(struct connection *con,
    msgpack_object *obj);
static int connection_handle_response(struct connection *con,
    msgpack_object *obj);
static void connection_request_event(connection_request_event_info *info);

static msgpack_sbuffer sbuf;
equeue *equeue_root;

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


int connection_create(uv_stream_t *stream)
{
  stream->data = NULL;

  struct connection *con = MALLOC(struct connection);

  if (con == NULL)
    return (-1);

  con->id = id_cnt++;
  con->msgid = 1;
  con->mpac = msgpack_unpacker_new(MSGPACK_UNPACKER_INIT_BUFFER_SIZE);
  con->closed = false;
  con->queue = equeue_new(equeue_root);
  con->streams.read = inputstream_new(parse_cb, 1024, con);
  con->streams.write = outputstream_new(1024 * 1024);
  con->streams.uv = stream;

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
    else if (message_is_response(&result.data))
      connection_handle_response(con, &result.data);
    else {
      /* invalid message, send response with error */
    }
  }
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
  }

  FREE(eventinfo->request);
}


static int connection_handle_response(UNUSED(struct connection *con),
    UNUSED(msgpack_object *obj))
{
  return (0);
}
