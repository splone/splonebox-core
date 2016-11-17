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

#include "rpc/connection/connection.h"
#ifdef __linux__
#include <bsd/string.h>            // for strlcpy
#endif
#include <msgpack/object.h>        // for msgpack_object, msgpack_object_union
#include <msgpack/pack.h>          // for msgpack_packer_init, msgpack_packer
#include <msgpack/sbuffer.h>       // for msgpack_sbuffer, msgpack_sbuffer_c...
#include <msgpack/unpack.h>        // for msgpack_unpacked, msgpack_unpacked...
#include <stdbool.h>               // for true, bool, false
#include <stddef.h>                // for NULL, size_t
#include <stdint.h>                // for uint64_t, UINT64_MAX, uint32_t
#include <stdlib.h>                // for abort, exit, realloc
#include <string.h>                // for strlen
#include <uv.h>                    // for uv_handle_t, uv_close, uv_timer_t
#include "api/helpers.h"           // for NIL
#include "api/sb-api.h"            // for api_free_array, api_free_object
#include "khash.h"                 // for __i, khint32_t
#include "main.h"                  // for main_loop
#include "rpc/connection/event.h"  // for multiqueue_free, multiqueue_new_child
#include "rpc/connection/loop.h"   // for loop, LOOP_PROCESS_EVENTS_UNTIL
#include "rpc/msgpack/helpers.h"   // for msgpack_rpc_serialize_request, msg...
#include "rpc/sb-rpc.h"            // for callinfo, crypto_context, object
#include "tweetnacl.h"             // for randombytes


STATIC void parse_cb(inputstream *istream, void *data, bool eof);
STATIC void close_cb(uv_handle_t *handle);
STATIC void timer_cb(uv_timer_t *timer);
STATIC void connection_handle_request(struct connection *con,
    msgpack_object *obj);
STATIC void connection_handle_response(struct connection *con,
    msgpack_object *obj);
STATIC void connection_request_event(void **argv);
STATIC void connection_close(struct connection *con);
STATIC void call_set_error(struct connection *con, char *msg);
STATIC int is_valid_rpc_response(msgpack_object *obj, struct connection *con);
STATIC void free_connection(struct connection *con);
STATIC void incref(struct connection *con);
STATIC void decref(struct connection *con);
STATIC void unsubscribe(struct connection *con, char *event);
STATIC void send_delayed_notifications(struct connection *con);

static uint64_t next_con_id = 1;
static hashmap(uint64_t, ptr_t) *connections = NULL;
static hashmap(cstr_t, uint64_t) *pluginkeys = NULL;
static hashmap(cstr_t, ptr_t) *event_strings = NULL;
static msgpack_sbuffer sbuf;

int connection_init(void)
{
  connections = hashmap_new(uint64_t, ptr_t)();
  pluginkeys = hashmap_new(cstr_t, uint64_t)();
  event_strings = hashmap_new(cstr_t, ptr_t)();

  if (dispatch_table_init() == -1)
    return (-1);

  if (!connections || !pluginkeys || !event_strings)
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
  });


  hashmap_free(uint64_t, ptr_t)(connections);
  hashmap_free(cstr_t, uint64_t)(pluginkeys);
  hashmap_free(cstr_t, ptr_t)(event_strings);

  dispatch_teardown();
  msgpack_sbuffer_destroy(&sbuf);

  return (0);
}

int connection_create(uv_stream_t *stream)
{
  int r;

  stream->data = NULL;

  struct connection *con = MALLOC(struct connection);

  if (con == NULL)
    return (-1);

  con->id = next_con_id++;
  con->msgid = 1;
  con->refcount = 1;
  con->mpac = msgpack_unpacker_new(MSGPACK_UNPACKER_INIT_BUFFER_SIZE);
  con->closed = false;
  con->events = multiqueue_new_child(main_loop.events);
  con->streams.read = inputstream_new(parse_cb, STREAM_BUFFER_SIZE, con);
  con->streams.write = outputstream_new(1024 * 1024);
  con->streams.uv = stream;
  con->cc.nonce = (uint64_t) randommod(281474976710656LL);
  con->subscribed_events = hashmap_new(cstr_t, ptr_t)();
  con->pending_requests = 0;

  if (ISODD(con->cc.nonce)) {
    con->cc.nonce++;
  }

  con->cc.receivednonce = 0;
  con->cc.state = TUNNEL_INITIAL;

  /* crypto minutekey timer */
  randombytes(con->cc.minutekey, sizeof con->cc.minutekey);
  randombytes(con->cc.lastminutekey, sizeof con->cc.lastminutekey);
  con->minutekey_timer.data = &con->cc;
  r = uv_timer_init(&main_loop.uv, &con->minutekey_timer);
  sbassert(r == 0);
  r = uv_timer_start(&con->minutekey_timer, timer_cb, 60000, 60000);
  sbassert(r == 0);

  con->packet.data = NULL;
  con->packet.start = 0;
  con->packet.end = 0;
  con->packet.pos = 0;

  kv_init(con->callvector);
  kv_init(con->delayed_notifications);

  inputstream_set(con->streams.read, stream);
  inputstream_start(con->streams.read);
  outputstream_set(con->streams.write, stream);

  hashmap_put(uint64_t, ptr_t)(connections, con->id, con);

  return (0);
}

void connection_subscribe(uint64_t id, char *event)
{
  struct connection *con;

  if (!(con = hashmap_get(uint64_t, ptr_t)(connections, id)) || con->closed)
    abort();

  char *event_string = hashmap_get(cstr_t, ptr_t)(event_strings, event);

  if (!event_string) {
    event_string = box_strdup(event);
    hashmap_put(cstr_t, ptr_t)(event_strings, event_string, event_string);
  }

  hashmap_put(cstr_t, ptr_t)(con->subscribed_events, event_string, event_string);
}

void connection_unsubscribe(uint64_t id, char *event)
{
  struct connection *con;

  if (!(con = hashmap_get(uint64_t, ptr_t)(connections, id)) || con->closed)
    abort();

  unsubscribe(con, event);
}

STATIC void broadcast_event(char *name, array args)
{
  kvec_t(struct connection *) subscribed  = KV_INITIAL_VALUE;
  struct connection *con;
  msgpack_packer packer;

  hashmap_foreach_value(connections, con, {
    if (hashmap_has(cstr_t, ptr_t)(con->subscribed_events, name)) {
      kv_push(subscribed, con);
    }
  });

  if (!kv_size(subscribed)) {
    api_free_array(args);
    goto end;
  }

  string method = {.length = strlen(name), .str = name};

  msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);
  msgpack_rpc_serialize_request(0, method, args, &packer);
  api_free_array(args);

  for (size_t i = 0; i < kv_size(subscribed); i++) {
    con = kv_A(subscribed, i);

    if (con->pending_requests) {
      wbuffer *rv = MALLOC(wbuffer);
      rv->size = sbuf.size;
      rv->data = sb_memdup_nulterm(sbuf.data, sbuf.size);
      kv_push(con->delayed_notifications, rv);
    } else {
      crypto_write(&con->cc, sbuf.data, sbuf.size, con->streams.write);
    }
  }

  msgpack_sbuffer_clear(&sbuf);

end:
  kv_destroy(subscribed);
}

STATIC void unsubscribe(struct connection *con, char *event)
{
  char *event_string = hashmap_get(cstr_t, ptr_t)(event_strings, event);
  hashmap_del(cstr_t, ptr_t)(con->subscribed_events, event_string);

  hashmap_foreach_value(connections, con, {
    if (hashmap_has(cstr_t, ptr_t)(con->subscribed_events, event_string)) {
      return;
    }
  });

  hashmap_del(cstr_t, ptr_t)(event_strings, event_string);
  FREE(event_string);
}

STATIC void incref(struct connection *con)
{
  con->refcount++;
}

STATIC void decref(struct connection *con)
{
  if (!(--con->refcount)) {
    free_connection(con);
  }
}


void connection_hashmap_put(uint64_t id, struct connection *con)
{
  hashmap_put(uint64_t, ptr_t)(connections, id, con);
}

void pluginkeys_hashmap_put(char *pluginkey, uint64_t id)
{
  hashmap_put(cstr_t, uint64_t)(pluginkeys, pluginkey, id);
}

STATIC void free_connection(struct connection *con)
{
  hashmap_del(uint64_t, ptr_t)(connections, con->id);
  hashmap_del(cstr_t, uint64_t)(pluginkeys, con->cc.pluginkeystring);
  msgpack_unpacker_free(con->mpac);

  char *event_string;
  hashmap_foreach_value(con->subscribed_events, event_string, {
    unsubscribe(con, event_string);
  });

  hashmap_free(cstr_t, ptr_t)(con->subscribed_events);
  kv_destroy(con->callvector);
  kv_destroy(con->delayed_notifications);
  multiqueue_free(con->events);

  if (con->packet.data)
    FREE(con->packet.data);

  FREE(con);
}

STATIC void timer_cb(uv_timer_t *timer)
{
  struct crypto_context *cc = (struct crypto_context*)timer->data;
  crypto_update_minutekey(cc);
}

STATIC void connection_close(struct connection *con)
{
  uv_handle_t *handle;
  uv_handle_t *timer_handle;

  if (con->closed)
    return;

  con->closed = true;

  timer_handle = (uv_handle_t*) &con->minutekey_timer;
  if (timer_handle) {
    uv_close(timer_handle, NULL);
    uv_run(&main_loop.uv, UV_RUN_ONCE);
  }

  inputstream_free(con->streams.read);
  outputstream_free(con->streams.write);
  handle = (uv_handle_t *)con->streams.uv;

  if (handle)
    uv_close(handle, close_cb);

  decref(con);
}


STATIC void close_cb(uv_handle_t *handle)
{
  FREE(handle->data);
  FREE(handle);
}

STATIC void reset_packet(struct connection *con)
{
  con->packet.start = 0;
  con->packet.end = 0;
  con->packet.pos = 0;
}

STATIC void reset_parser(struct connection *con)
{
  FREE(con->packet.data);
  reset_packet(con);
}

STATIC bool is_rpc_response(msgpack_object *obj)
{
  return obj->type == MSGPACK_OBJECT_ARRAY
      && obj->via.array.size == 4
      && obj->via.array.ptr[0].type == MSGPACK_OBJECT_POSITIVE_INTEGER
      && obj->via.array.ptr[0].via.u64 == 1
      && obj->via.array.ptr[1].type == MSGPACK_OBJECT_POSITIVE_INTEGER;
}

STATIC void send_error(struct connection *con, uint64_t id, char *err)
{
  struct api_error e = ERROR_INIT;

  error_set(&e, API_ERROR_TYPE_VALIDATION, "%s", err);

  msgpack_packer pac;
  msgpack_packer_init(&pac, &sbuf, msgpack_sbuffer_write);
  msgpack_rpc_serialize_response(id, &e, NIL, &pac);

  crypto_write(&con->cc, sbuf.data, sbuf.size, con->streams.write);

  msgpack_sbuffer_clear(&sbuf);
}

STATIC void parse_cb(inputstream *istream, void *data, bool eof)
{
  unsigned char *packet;
  unsigned char hellopacket[192];
  unsigned char initiatepacket[256];
  struct connection *con = data;

  incref(con);

  size_t read = 0;
  size_t pending;
  size_t size;
  uint64_t plaintextlen;
  uint64_t consumedlen = 0;
  uint64_t dummylen = 0;
  msgpack_unpacked result;

  if (eof) {
    connection_close(con);
    goto end;
  }

  if (con->cc.state == TUNNEL_INITIAL) {
    size = inputstream_read(istream, hellopacket, 192);
    if (crypto_recv_hello_send_cookie(&con->cc, hellopacket,
        con->streams.write) != 0)
      LOG_WARNING("establishing crypto tunnel failed at hello-cookie packet");

    goto end;
  } else if (con->cc.state == TUNNEL_COOKIE_SENT) {
    size = inputstream_read(istream, initiatepacket, 256);
    if (crypto_recv_initiate(&con->cc, initiatepacket) != 0) {
      LOG_WARNING("establishing crypto tunnel failed at initiate packet");
      con->cc.state = TUNNEL_INITIAL;
    }

    if (hashmap_has(cstr_t, uint64_t)(pluginkeys,
        con->cc.pluginkeystring)) {
      LOG_WARNING("pluginkey already registered, closing connection");
      sbmemzero(con->cc.pluginkeystring,
          sizeof con->cc.pluginkeystring);
      connection_close(con);
      goto end;
    }

    hashmap_put(cstr_t, uint64_t)(pluginkeys, con->cc.pluginkeystring,
      con->id);
  }

  pending = inputstream_pending(istream);

  if (pending <= 0 || con->cc.state != TUNNEL_ESTABLISHED)
    goto end;

  if (con->packet.end <= 0) {
    packet = inputstream_get_read(istream, &read);

    /* read the packet length */
    if (crypto_verify_header(&con->cc, packet, &con->packet.length)) {
      reset_packet(con);
      goto end;
    }

    con->packet.end = con->packet.length;
    con->packet.data = MALLOC_ARRAY(MAX(con->packet.end, read), unsigned char);

    if (!con->packet.data) {
      LOG_ERROR("Failed to alloc mem for con packet.");
      goto end;
    }

    if (msgpack_unpacker_reserve_buffer(con->mpac,
      MAX(read, con->packet.end)) == false) {
      LOG_ERROR("Failed to reserve mem msgpack buffer.");
      goto end;
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
        goto end;
      }

      consumedlen += plaintextlen;
      packet = inputstream_get_read(istream, &dummylen);

      if (packet == NULL) {
        reset_parser(con);
        goto end;
      }

      if (crypto_verify_header(&con->cc, packet, &con->packet.length)) {
        reset_parser(con);
        goto end;
      }

      con->packet.end = con->packet.length;

      continue;
    }

    if (con->packet.end > 0 && read == 0) {
      goto end;
    }

    if (crypto_read(&con->cc, con->packet.data, con->unpackbuf +
        consumedlen, con->packet.length, &plaintextlen) != 0) {
      reset_parser(con);
      goto end;
    }

    consumedlen += plaintextlen;
    reset_parser(con);
  }

  msgpack_unpacker_buffer_consumed(con->mpac, consumedlen);
  msgpack_unpacked_init(&result);
  msgpack_unpack_return ret;

  /* deserialize objects, one by one */
  while ((ret =
      msgpack_unpacker_next(con->mpac, &result)) == MSGPACK_UNPACK_SUCCESS) {
    bool is_response = is_rpc_response(&result.data);

    if (is_response) {
      if (is_valid_rpc_response(&result.data, con)) {
        connection_handle_response(con, &result.data);
      } else {
        call_set_error(con, "Returned response that doesn't have a matching "
                            "request id. Ensure the client is properly "
                            "synchronized");
      }

      msgpack_unpacked_destroy(&result);
      goto end;
    }

    connection_handle_request(con, &result.data);
  }

  if (ret == MSGPACK_UNPACK_NOMEM_ERROR) {
    decref(con);
    exit(2);
  }

  if (ret == MSGPACK_UNPACK_PARSE_ERROR) {
    send_error(con, 0, "Invalid msgpack payload. "
        "This error can also happen when deserializing "
        "an object with high level of nesting");
  }

end:
  decref(con);
}

bool connection_send_event(uint64_t id, char *name, array args)
{
  msgpack_packer packer;
  struct connection *con = NULL;

  if (id && (!(con = hashmap_get(uint64_t, ptr_t)(connections, id))
      || con->closed)) {
    api_free_array(args);
    return false;
  }

  if (con) {
    string method = cstring_to_string(name);
    msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);
    msgpack_rpc_serialize_request(0, method, args, &packer);
    api_free_array(args);

    if (con->pending_requests) {
      wbuffer *rv = MALLOC(wbuffer);
      rv->size = sbuf.size;
      rv->data = sb_memdup_nulterm(sbuf.data, sbuf.size);
      kv_push(con->delayed_notifications, rv);
    } else {
      crypto_write(&con->cc, sbuf.data, sbuf.size, con->streams.write);
    }

    msgpack_sbuffer_clear(&sbuf);
  } else {
    broadcast_event(name, args);
  }

  return true;
}


object connection_send_request(char *pluginkey, string method,
    array args, struct api_error *err)
{
  uint64_t id;
  struct connection *con;
  msgpack_packer packer;

  id = hashmap_get(cstr_t, uint64_t)(pluginkeys, pluginkey);

  if (id == 0) {
    api_free_array(args);
    error_set(err, API_ERROR_TYPE_VALIDATION, "plugin not registered");
    return NIL;
  }

  con = hashmap_get(uint64_t, ptr_t)(connections, id);

  /*
   * if no connection is available for the key, set the connection to the
   * the initial connection from the sender.
   */
  if (!con) {
    api_free_array(args);
    error_set(err, API_ERROR_TYPE_VALIDATION, "plugin not registered");
    return NIL;
  }

  incref(con);

  uint64_t msgid = con->msgid++;

  msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);
  msgpack_rpc_serialize_request(msgid, method, args, &packer);

  api_free_array(args);

  LOG_VERBOSE(VERBOSE_LEVEL_0, "sending request: method = %s,  callinfo id = %u\n",
      method.str, con->msgid);
  if (crypto_write(&con->cc, sbuf.data, sbuf.size, con->streams.write) != 0)
    return NIL;

  msgpack_sbuffer_clear(&sbuf);

  struct callinfo cinfo = (struct callinfo) { msgid, false, false, NIL };

  loop_process_events_until(&main_loop, con, &cinfo);

  if (cinfo.errored) {
    if (cinfo.result.type == OBJECT_TYPE_STR) {
      error_set(err, API_ERROR_TYPE_EXCEPTION, "%s",
          cinfo.result.data.string.str);
    } else if (cinfo.result.type == OBJECT_TYPE_ARRAY) {
      array array = cinfo.result.data.array;

      if (array.size == 2 && array.items[0].type == OBJECT_TYPE_INT
          && (array.items[0].data.integer == API_ERROR_TYPE_EXCEPTION
              || array.items[0].data.integer == API_ERROR_TYPE_VALIDATION)
          && array.items[1].type == OBJECT_TYPE_STR) {
        err->type = (api_error_type) array.items[0].data.integer;
        strlcpy(err->msg, array.items[1].data.string.str, sizeof(err->msg));
        err->isset = true;
      } else {
        error_set(err, API_ERROR_TYPE_EXCEPTION, "%s", "unknown error");
      }
    } else {
      error_set(err, API_ERROR_TYPE_EXCEPTION, "%s", "unknown error");
    }

    api_free_object(cinfo.result);
  }

  if (!con->pending_requests) {
    send_delayed_notifications(con);
  }

  decref(con);

  return cinfo.errored ? NIL : cinfo.result;
}

STATIC void send_delayed_notifications(struct connection *con)
{
  for (size_t i = 0; i < kv_size(con->delayed_notifications); i++) {
    wbuffer *buffer = kv_A(con->delayed_notifications, i);
    crypto_write(&con->cc, buffer->data, buffer->size, con->streams.write);
    FREE(buffer->data);
    FREE(buffer);
  }

  kv_size(con->delayed_notifications) = 0;
}

int connection_send_response(uint64_t con_id, uint32_t msgid,
    object arg, struct api_error *api_error)
{
  msgpack_packer packer;
  struct connection *con;

  con = hashmap_get(uint64_t, ptr_t)(connections, con_id);

  /*
   * if no connection is available for the key, set the connection to the
   * the initial connection from the sender.
   */
  if (!con) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "plugin not registered");
    return (-1);
  }

  msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);
  msgpack_rpc_serialize_response(msgid, api_error, arg, &packer);

  if (api_error->isset) {
    return (-1);
  }

  if (crypto_write(&con->cc, sbuf.data, sbuf.size, con->streams.write) != 0) {
    return (-1);
  }

  msgpack_sbuffer_clear(&sbuf);
  api_free_object(arg);

  return 0;
}


STATIC void connection_handle_request(struct connection *con,
    msgpack_object *obj)
{
  array args = ARRAY_DICT_INIT;
  uint64_t msgid;
  dispatch_info dispatcher;
  msgpack_object *method;
  struct api_error api_error = ERROR_INIT;

  msgpack_rpc_validate(&msgid, obj, &api_error);

  if (api_error.isset) {
    send_error(con, msgid, "Invalid message from connection, closed");
  }

  method = msgpack_rpc_method(obj);

  if (method) {
    dispatcher = msgpack_rpc_get_handler_for(method->via.bin.ptr,
          method->via.bin.size);
  } else {
    dispatcher.func = msgpack_rpc_handle_missing_method;
    dispatcher.async = true;
  }

  if (!msgpack_rpc_to_array(msgpack_rpc_args(obj), &args)) {
    dispatcher.func = msgpack_rpc_handle_invalid_arguments;
    dispatcher.async = true;
  }

  connection_request_event_info *eventinfo = MALLOC(connection_request_event_info);
  eventinfo->con = con;
  eventinfo->dispatcher = dispatcher;
  eventinfo->args = args;
  eventinfo->msgid = msgid;

  incref(con);

  if (dispatcher.async)
    connection_request_event((void**)&eventinfo);
  else {
    multiqueue_put(con->events, connection_request_event, 1, eventinfo);
  }
}


STATIC void connection_request_event(void **argv)
{
  connection_request_event_info *eventinfo = argv[0];
  object result;
  msgpack_packer packer;
  struct connection *con;
  struct api_error error = ERROR_INIT;

  con = eventinfo->con;
  array args = eventinfo->args;
  uint64_t msgid = eventinfo->msgid;
  dispatch_info handler = eventinfo->dispatcher;

  result = handler.func(con->id, msgid, con->cc.pluginkeystring, args, &error);

  if (eventinfo->msgid != UINT64_MAX) {
    msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);
    msgpack_rpc_serialize_response(msgid, &error, result, &packer);
    crypto_write(&eventinfo->con->cc, sbuf.data, sbuf.size,
        eventinfo->con->streams.write);
    msgpack_sbuffer_clear(&sbuf);
  } else {
    api_free_object(result);
  }

  api_free_array(args);

  decref(con);
  FREE(eventinfo);
}

STATIC int is_valid_rpc_response(msgpack_object *obj, struct connection *con)
{
  uint64_t msg_id = obj->via.array.ptr[1].via.u64;

  return kv_size(con->callvector) && msg_id
      == kv_A(con->callvector, kv_size(con->callvector) - 1)->msgid;
}


STATIC void connection_handle_response(struct connection *con,
    msgpack_object *obj)
{
  struct callinfo *cinfo;

  cinfo = kv_A(con->callvector, kv_size(con->callvector) - 1);

  LOG_VERBOSE(VERBOSE_LEVEL_0, "received response: callinfo id = %lu\n",
      cinfo->msgid);

  cinfo->returned = true;
  cinfo->errored = (obj->via.array.ptr[2].type != MSGPACK_OBJECT_NIL);

  if (cinfo->errored) {
    msgpack_rpc_to_object(&obj->via.array.ptr[2], &cinfo->result);
  } else {
    msgpack_rpc_to_object(&obj->via.array.ptr[3], &cinfo->result);
  }
}

STATIC void call_set_error(struct connection *con, UNUSED(char *msg))
{
  struct callinfo *cinfo;

  for (size_t i = 0; i < kv_size(con->callvector); i++) {
      cinfo = kv_A(con->callvector, i);
      cinfo->errored = true;
      cinfo->returned = true;
  }

  connection_close(con);
}
