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

#include "rpc/connection/loop.h"
#include <stdbool.h>     // for bool
#include <stdlib.h>      // for NULL, abort
#include "rpc/sb-rpc.h"  // for event

static void async_cb(uv_async_t *handle);
static void timer_cb(uv_timer_t *handle);

void loop_init(UNUSED(loop *loop), UNUSED(void *data))
{
  uv_loop_init(&loop->uv);
  loop->recursive = 0;
  loop->uv.data = loop;
  loop->children = kl_init(WatcherPtr);
  loop->children_stop_requests = 0;
  loop->events = multiqueue_new_parent(loop_on_put, loop);
  loop->fast_events = multiqueue_new_child(loop->events);
  loop->thread_events = multiqueue_new_parent(NULL, NULL);
  uv_mutex_init(&loop->mutex);
  uv_async_init(&loop->uv, &loop->async, async_cb);
  uv_signal_init(&loop->uv, &loop->children_watcher);
  uv_timer_init(&loop->uv, &loop->children_kill_timer);
  uv_timer_init(&loop->uv, &loop->poll_timer);
}

void loop_poll_events(loop *loop, int ms)
{
  if (loop->recursive++) {
    abort();
  }

  uv_run_mode mode = UV_RUN_ONCE;

  if (ms > 0) {
    uv_timer_start(&loop->poll_timer, timer_cb, (uint64_t)ms, (uint64_t)ms);
  } else if (ms == 0) {
    mode = UV_RUN_NOWAIT;
  }

  uv_run(&loop->uv, mode);

  if (ms > 0) {
    uv_timer_stop(&loop->poll_timer);
  }

  loop->recursive--;  // Can re-enter uv_run now
  multiqueue_process_events(loop->fast_events);
}

// Schedule an event from another thread
void loop_schedule(loop *loop, event event)
{
  uv_mutex_lock(&loop->mutex);
  multiqueue_put_event(loop->thread_events, event);
  uv_async_send(&loop->async);
  uv_mutex_unlock(&loop->mutex);
}

void loop_on_put(UNUSED(multiqueue *queue), void *data)
{
  loop *loop = data;
  // Sometimes libuv will run pending callbacks(timer for example) before
  // blocking for a poll. If this happens and the callback pushes a event to one
  // of the queues, the event would only be processed after the poll
  // returns(user hits a key for example). To avoid this scenario, we call
  // uv_stop when a event is enqueued.
  uv_stop(&loop->uv);
}

void loop_close(loop *loop, bool wait)
{
  uv_mutex_destroy(&loop->mutex);
  uv_close((uv_handle_t *)&loop->children_watcher, NULL);
  uv_close((uv_handle_t *)&loop->children_kill_timer, NULL);
  uv_close((uv_handle_t *)&loop->poll_timer, NULL);
  uv_close((uv_handle_t *)&loop->async, NULL);
  do {
    uv_run(&loop->uv, wait ? UV_RUN_DEFAULT : UV_RUN_NOWAIT);
  } while (uv_loop_close(&loop->uv) && wait);
  multiqueue_free(loop->fast_events);
  multiqueue_free(loop->thread_events);
  multiqueue_free(loop->events);
  kl_destroy(WatcherPtr, loop->children);
}

static void async_cb(uv_async_t *handle)
{
  loop *l = handle->loop->data;
  uv_mutex_lock(&l->mutex);
  while (!multiqueue_empty(l->thread_events)) {
    event ev = multiqueue_get(l->thread_events);
    multiqueue_put_event(l->fast_events, ev);
  }
  uv_mutex_unlock(&l->mutex);
}

static void timer_cb(UNUSED(uv_timer_t *handle))
{
}

void loop_process_events_until(loop *loop, struct connection *con,
    struct callinfo *cinfo)
{
  /* push callinfo to connection callinfo vector */
  kv_push(con->callvector, cinfo);
  con->pending_requests++;

  /* wait until requestinfo returned, in time process events */
  LOOP_PROCESS_EVENTS_UNTIL(loop, con->events, -1, cinfo->returned);

  /* delete last from callinfo vector */
  (void)kv_pop(con->callvector);
  con->pending_requests--;
}
