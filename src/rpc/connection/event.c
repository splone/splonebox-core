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

#include "rpc/connection/event.h"
#include <stdbool.h>    // for bool, false, true
#include <stddef.h>     // for NULL
#include "queue.h"      // for QUEUE_REMOVE, QUEUE, QUEUE_EMPTY, QUEUE_INSER...
#include "sb-common.h"  // for sbassert, FREE, MALLOC

typedef struct multiqueue_item multiqueueitem;

struct multiqueue_item {
  union {
    multiqueue *queue;
    struct {
      event event;
      multiqueueitem *parent;
    } item;
  } data;
  bool link;  // true: current item is just a link to a node in a child queue
  QUEUE node;
};

struct multiqueue {
  multiqueue *parent;
  QUEUE headtail;
  put_callback put_cb;
  void *data;
};

static multiqueue *multiqueue_new(multiqueue *parent, put_callback put_cb, void *data);
static event multiqueue_remove(multiqueue *this);
static void multiqueue_push(multiqueue *this, event e);
static multiqueueitem *multiqueue_node_data(QUEUE *q);

static event NILEVENT = { .handler = NULL, .argv = {NULL} };

multiqueue *multiqueue_new_parent(put_callback put_cb, void *data)
{
  return multiqueue_new(NULL, put_cb, data);
}

multiqueue *multiqueue_new_child(multiqueue *parent)
{
  sbassert(!parent->parent);
  return multiqueue_new(parent, NULL, NULL);
}

static multiqueue *multiqueue_new(multiqueue *parent, put_callback put_cb,
    void *data)
{
  multiqueue *rv = malloc_or_die(sizeof(multiqueue));
  QUEUE_INIT(&rv->headtail);
  rv->parent = parent;
  rv->put_cb = put_cb;
  rv->data = data;
  return rv;
}

void multiqueue_free(multiqueue *this)
{
  sbassert(this);
  while (!QUEUE_EMPTY(&this->headtail)) {
    QUEUE *q = QUEUE_HEAD(&this->headtail);
    multiqueueitem *item = multiqueue_node_data(q);
    if (this->parent) {
      QUEUE_REMOVE(&item->data.item.parent->node);
      FREE(item->data.item.parent);
    }
    QUEUE_REMOVE(q);
    FREE(item);
  }

  FREE(this);
}

event multiqueue_get(multiqueue *this)
{
  return multiqueue_empty(this) ? NILEVENT : multiqueue_remove(this);
}

void multiqueue_put_event(multiqueue *this, event e)
{
  sbassert(this);
  multiqueue_push(this, e);
  if (this->parent && this->parent->put_cb) {
    this->parent->put_cb(this->parent, this->parent->data);
  }
}

void multiqueue_process_events(multiqueue *this)
{
  sbassert(this);
  while (!multiqueue_empty(this)) {
    event e = multiqueue_get(this);
    if (e.handler) {
      e.handler(e.argv);
    }
  }
}

bool multiqueue_empty(multiqueue *this)
{
  sbassert(this);
  return QUEUE_EMPTY(&this->headtail);
}

void multiqueue_replace_parent(multiqueue *this, multiqueue *new_parent)
{
  sbassert(multiqueue_empty(this));
  this->parent = new_parent;
}

static event multiqueue_remove(multiqueue *this)
{
  sbassert(!multiqueue_empty(this));
  QUEUE *h = QUEUE_HEAD(&this->headtail);
  QUEUE_REMOVE(h);
  multiqueueitem *item = multiqueue_node_data(h);
  event rv;

  if (item->link) {
    sbassert(!this->parent);
    // remove the next node in the linked queue
    multiqueue *linked = item->data.queue;
    sbassert(!multiqueue_empty(linked));
    multiqueueitem *child =
      multiqueue_node_data(QUEUE_HEAD(&linked->headtail));
    QUEUE_REMOVE(&child->node);
    rv = child->data.item.event;
    FREE(child);
  } else {
    if (this->parent) {
      // remove the corresponding link node in the parent queue
      QUEUE_REMOVE(&item->data.item.parent->node);
      FREE(item->data.item.parent);
    }
    rv = item->data.item.event;
  }

  FREE(item);
  return rv;
}

static void multiqueue_push(multiqueue *this, event e)
{
  multiqueueitem *item = malloc_or_die(sizeof(multiqueueitem));
  item->link = false;
  item->data.item.event = e;
  QUEUE_INSERT_TAIL(&this->headtail, &item->node);

  if (this->parent) {
    // push link node to the parent queue
    item->data.item.parent = malloc_or_die(sizeof(multiqueueitem));
    item->data.item.parent->link = true;
    item->data.item.parent->data.queue = this;
    QUEUE_INSERT_TAIL(&this->parent->headtail, &item->data.item.parent->node);
  }
}

static multiqueueitem *multiqueue_node_data(QUEUE *q)
{
  return QUEUE_DATA(q, multiqueueitem, node);
}
