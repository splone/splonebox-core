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
#include <uv.h>

#include "tweetnacl.h"
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

static hashmap(string, ptr_t) *connections = NULL;
static msgpack_sbuffer sbuf;
equeue *equeue_root;
uv_loop_t loop;

int connection_init(void)
{
  connections = hashmap_new(string, ptr_t)();

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

  hashmap_foreach_value(connections, con, {
    connection_close(con);
    FREE(con);
  });

  hashmap_free(string, ptr_t)(connections);
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
  con->streams.read = inputstream_new(parse_cb, STREAM_BUFFER_SIZE, con);
  con->streams.write = outputstream_new(1024 * 1024);
  con->streams.uv = stream;
  con->cc.nonce = (uint64_t) randommod(281474976710656LL);

  if (ISODD(con->cc.nonce)) {
    con->cc.nonce++;
  }

  con->cc.receivednonce = 0;
  con->cc.state = TUNNEL_INITIAL;
  con->packet.start = 0;
  con->packet.end = 0;
  con->packet.pos = 0;

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

static void reset_packet(struct connection *con)
{
  con->packet.start = 0;
  con->packet.end = 0;
  con->packet.pos = 0;
}

static void reset_parser(struct connection *con)
{
  FREE(con->packet.data);
  msgpack_unpacker_free(con->mpac);
  reset_packet(con);
}

static void parse_cb(inputstream *istream, void *data, bool eof)
{
//  unsigned char *tunnelpacket;
  unsigned char *packet;
  struct connection *con = data;
  size_t read = 0;
  size_t pending;
  size_t size;
  uint64_t plaintextlen;
  uint64_t consumedlen = 0;
  uint64_t dummylen = 0;
  msgpack_unpacked result;

  if (eof) {
    connection_close(con);
    return;
  }

  pending = inputstream_pending(istream);

  if (con->cc.state == TUNNEL_INITIAL) {
    unsigned char tunnelpacket[pending];
//    tunnelpacket = MALLOC_ARRAY(pending, unsigned char);
    size = inputstream_read(istream, &tunnelpacket, pending);
    if (crypto_tunnel(&con->cc, tunnelpacket, con->streams.write) != 0)
      LOG_WARNING("establishing crypto tunnel failed");
//    FREE(tunnelpacket);
    return;
  }

  if (con->packet.end <= 0) {
    packet = inputstream_get_read(istream, &read);

    /* read the packet length */
    if (crypto_verify_header(packet, &con->packet.length)) {
      reset_packet(con);
      return;
    }

    con->packet.end = con->packet.length;
    con->packet.data = MALLOC_ARRAY(MAX(con->packet.end, read), unsigned char);
    if (!con->packet.data) {
      LOG_ERROR("Failed to alloc mem for con packet.");
      return;
    }

    if (msgpack_unpacker_reserve_buffer(con->mpac,
      MAX(read, con->packet.end)) == false) {
      LOG_ERROR("Failed to reserve mem msgpack buffer.");
      return;
    };

    /* get decrypted message start position */
    con->unpackbuf = msgpack_unpacker_buffer(con->mpac);
  }

  while(read > 0) {
    con->packet.start = inputstream_read(istream,
      con->packet.data + con->packet.pos, con->packet.end);
    con->packet.pos += con->packet.start;
    con->packet.end -= con->packet.start;
    read -= con->packet.start;

    if (read > 0 && con->packet.end == 0) {
      if (crypto_read(&con->cc, con->packet.data, con->unpackbuf +
          consumedlen, con->packet.length, &plaintextlen) != 0) {
        reset_parser(con);
        return;
      }

      consumedlen += plaintextlen;
      packet = inputstream_get_read(istream, &dummylen);

      if (packet == NULL) {
        reset_parser(con);
        return;
      }

      if (crypto_verify_header(packet, &con->packet.length)) {
        reset_parser(con);
        return;
      }

      con->packet.end = con->packet.length;

      continue;
    }

    if (con->packet.end > 0 && read == 0)
      return;

    if (crypto_read(&con->cc, con->packet.data, con->unpackbuf +
        consumedlen, con->packet.length, &plaintextlen) != 0) {
      reset_parser(con);
      return;
    }

    consumedlen += plaintextlen;
    reset_packet(con);
    FREE(con->packet.data);
  }

  msgpack_unpacker_buffer_consumed(con->mpac, consumedlen);
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
      LOG_WARNING("invalid msgpack object");
      msgpack_object_print(stdout, result.data);
    }
  }
}

int connection_hashmap_put(string pluginlongtermpk, struct connection *con)
{
  hashmap_put(string, ptr_t)(connections, pluginlongtermpk, con);

  return (0);
}

struct callinfo * connection_send_request(string pluginlongtermpk, string method,
    array params, struct api_error *api_error)
{
  struct connection *con;
  msgpack_packer packer;
  struct message_request request;
  struct callinfo *cinfo;

  con = hashmap_get(string, ptr_t)(connections, pluginlongtermpk);

  /*
   * if no connection is available for the key, set the connection to the
   * the initial connection from the sender.
   */
  if (!con) {
    free_params(params);
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "plugin not registered");
    return (NULL);
  }

  request.type = MESSAGE_TYPE_REQUEST;
  request.msgid = con->msgid++;
  request.method = method;
  request.params = params;

  msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);
  message_serialize_request(&request, &packer);
  free_params(params);

  /* if error is set, generate an error response message */
  if (api_error->isset)
    return (NULL);

  if (crypto_write(&con->cc, sbuf.data, sbuf.size, con->streams.write) != 0)
    return (NULL);

  cinfo = loop_wait_for_response(con, &request);

  msgpack_sbuffer_clear(&sbuf);

  return cinfo;
}

int connection_send_response(struct connection *con, uint32_t msgid,
    array params, struct api_error *api_error)
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
  response.params = params;

  msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);
  message_serialize_response(&response, &packer);
  free_params(params);

  if (api_error->isset)
    return (-1);

  if (crypto_write(&con->cc, sbuf.data, sbuf.size, con->streams.write) != 0)
    return (-1);

  msgpack_sbuffer_clear(&sbuf);

  return 0;
}


static int connection_handle_request(struct connection *con,
    msgpack_object *obj)
{
  dispatch_info dispatcher;
  struct api_error api_error = ERROR_INIT;
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

  if (dispatcher.func == NULL) {
    error_set(&api_error, API_ERROR_TYPE_VALIDATION, "could not dispatch method");
    dispatcher.func = handle_error;
    dispatcher.async = true;
  }

  LOG_VERBOSE(VERBOSE_LEVEL_0, "received request: method = %s\n",
      eventinfo.request->method.str);

  eventinfo.con = con;
  eventinfo.api_error = api_error;
  eventinfo.dispatcher = dispatcher;

  if (dispatcher.async)
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
  msgpack_packer packer;

  eventinfo->dispatcher.func(eventinfo);

  if (eventinfo->api_error.isset) {
    msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);
    message_serialize_error_response(&packer, &eventinfo->api_error, eventinfo->request->msgid);

    crypto_write(&eventinfo->con->cc, sbuf.data, sbuf.size,
        eventinfo->con->streams.write);

    msgpack_sbuffer_clear(&sbuf);
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
