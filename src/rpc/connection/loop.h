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

 #pragma once

#include <stdbool.h>                    // for bool
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint64_t
#include <uv.h>                         // for uv_hrtime, uv_timer_t, uv_asy...
#include "klist.h"                      // for mp, kl, k, p, KLIST_INIT, kli...
#include "kvec.h"                       // for connection::(anonymous), kv_pop
#include "rpc/connection/connection.h"  // for connection
#include "rpc/connection/event.h"       // for multiqueue, multiqueue_empty
#include "rpc/connection/event-defs.h"
#include "rpc/sb-rpc.h"                 // for callinfo, event


typedef void * WatcherPtr;

#define _noop(x)
KLIST_INIT(WatcherPtr, WatcherPtr, _noop)

typedef struct loop {
  uv_loop_t uv;
  multiqueue *events, *fast_events, *thread_events;
  klist_t(WatcherPtr) *children;
  uv_signal_t children_watcher;
  uv_timer_t children_kill_timer, poll_timer;
  size_t children_stop_requests;
  uv_async_t async;
  uv_mutex_t mutex;
  int recursive;
} loop;

#define CREATE_EVENT(multiqueue, handler, argc, ...) \
  do { \
    if (multiqueue) { \
      multiqueue_put((multiqueue), (handler), argc, __VA_ARGS__); \
    } else { \
      void *argv[argc] = { __VA_ARGS__ }; \
      (handler)(argv); \
    } \
  } while (0)

// Poll for events until a condition or timeout
#define LOOP_PROCESS_EVENTS_UNTIL(loop, multiqueue, timeout, condition) \
  do { \
    int remaining = timeout; \
    uint64_t before = (remaining > 0) ? uv_hrtime() : 0; \
    while (!(condition)) { \
      LOOP_PROCESS_EVENTS(loop, multiqueue, remaining); \
      if (remaining == 0) { \
        break; \
      } else if (remaining > 0) { \
        uint64_t now = uv_hrtime(); \
        remaining -= (int) ((now - before) / 1000000); \
        before = now; \
        if (remaining <= 0) { \
          break; \
        } \
      } \
    } \
  } while (0)

#define LOOP_PROCESS_EVENTS(loop, multiqueue, timeout) \
  do { \
    if (multiqueue && !multiqueue_empty(multiqueue)) { \
      multiqueue_process_events(multiqueue); \
    } else { \
      loop_poll_events(loop, timeout); \
    } \
  } while (0)

void loop_init(loop *loop, void *data);
void loop_poll_events(loop *loop, int ms);
void loop_schedule(loop *loop, event e);
void loop_on_put(multiqueue *queue, void *data);
void loop_close(loop *loop, bool wait);
void loop_process_events_until(loop *loop, struct connection *con,
    struct callinfo *cinfo);
