/**
 *    Copyright (C) 2016 splone UG
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

#pragma once

#include <msgpack/sbuffer.h>       // for msgpack_sbuffer
#include <msgpack/unpack.h>        // for msgpack_unpacker
#include <stdbool.h>               // for bool
#include <stddef.h>                // for size_t
#include <stdint.h>                // for uint64_t, uint32_t
#include <uv.h>                    // for uv_stream_t, uv_timer_t
#include "kvec.h"                  // for kvec_t
#include "rpc/connection/event.h"  // for multiqueue
#include "rpc/sb-rpc.h"            // for crypto_context, hashmap_cstr_t_ptr_t
#include "sb-common.h"             // for hashmap

struct connection {
  uint64_t id;
  uint32_t msgid;
  size_t pending_requests;
  size_t refcount;
  msgpack_unpacker *mpac;
  msgpack_sbuffer *sbuf;
  char *unpackbuf;
  bool closed;
  multiqueue *events;
  struct {
    inputstream *read;
    outputstream *write;
    uv_stream_t *uv;
  } streams;
  kvec_t(struct callinfo *) callvector;
  kvec_t(wbuffer *) delayed_notifications;
  struct crypto_context cc;
  struct {
    uint64_t start;
    uint64_t end;
    uint64_t pos;
    uint64_t length;
    unsigned char *data;
  } packet;
  uv_timer_t minutekey_timer;
  hashmap(cstr_t, ptr_t) *subscribed_events;
};
