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

#include <stdbool.h>

#include "rpc/sb-rpc.h"
#include "sb-common.h"

static inline void loop_poll_events_until(struct connection *con,
    bool *condition);
equeue *equeue_root;
uv_loop_t loop;

static inline void loop_poll_events_until(struct connection *con,
    bool *condition)
{
  while (!*condition) {
    if (con->queue && !equeue_empty(con->queue)) {
      equeue_run_events(con->queue);
    } else {
      uv_run(&loop, UV_RUN_NOWAIT);
      equeue_run_events(equeue_root);
    }
  }
}


struct callinfo *loop_wait_for_response(struct connection *con,
    struct message_request *request)
{
  struct callinfo *cinfo;
  cinfo = MALLOC(struct callinfo);

  if (!cinfo)
    return (NULL);

  /* generate callinfo */
  cinfo->msgid = request->msgid;
  cinfo->hasresponse = false;
  cinfo->errorresponse = false;

  /* push callinfo to connection callinfo vector */
  kv_push(struct callinfo *, con->callvector, cinfo);
  con->pendingcalls++;

  /* wait until requestinfo returned, in time process events */
  loop_poll_events_until(con, &cinfo->hasresponse);

  /* delete last from callinfo vector */
  kv_pop(con->callvector);
  con->pendingcalls--;

  return cinfo;
}
