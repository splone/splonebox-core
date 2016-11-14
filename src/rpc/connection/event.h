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

#include <stdbool.h>     // for bool
#include "rpc/sb-rpc.h"  // for event


typedef struct multiqueue multiqueue;
typedef void (*put_callback)(multiqueue *multiq, void *data);

multiqueue *multiqueue_new_parent(put_callback put_cb, void *data);
multiqueue *multiqueue_new_child(multiqueue *parent);
void multiqueue_free(multiqueue *this);
event multiqueue_get(multiqueue *this);
void multiqueue_put_event(multiqueue *this, event event);
void multiqueue_process_events(multiqueue *this);
bool multiqueue_empty(multiqueue *this);
void multiqueue_replace_parent(multiqueue *this, multiqueue *new_parent);

#define multiqueue_put(q, h, ...) \
  multiqueue_put_event(q, event_create(1, h, __VA_ARGS__));
