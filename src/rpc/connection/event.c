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

#include <uv.h>

#include "sb-common.h"
#include "rpc/sb-rpc.h"

equeue *equeue_root;

static queue_entry *dequeue_child(equeue *queue);
static queue_entry *dequeue_child_from_root(equeue *queue);

int event_initialize(void)
{
  /* initialize event root queue */
  equeue_root = equeue_new(NULL);

  if (!equeue_root)
    return (-1);

  return 0;
}


equeue *equeue_new(equeue *root)
{
  equeue *queue = MALLOC(equeue);

  if (!queue)
    return (NULL);

  queue->root = root;

  TAILQ_INIT(&queue->head);

  return (queue);
}


/**
 * check if queue is empty
 *
 * @params queue queue instance
 */
bool equeue_empty(equeue *queue)
{
  return (TAILQ_EMPTY(&queue->head));
}


int equeue_put(equeue *queue, api_event event)
{
  if (!queue)
    return (-1);

  /* disallow `put` to root queue */
  if (!queue->root)
    return (-1);

  queue_entry *entry = MALLOC(queue_entry);
  entry->data.entry.event = event;
  TAILQ_INSERT_TAIL(&queue->head, entry, node);

  entry->data.entry.root = MALLOC(queue_entry);
  entry->data.entry.root->data.queue = queue;
  TAILQ_INSERT_TAIL(&queue->root->head, entry->data.entry.root, node);

  return (0);
}


api_event equeue_get(equeue *queue)
{
  api_event event;
  queue_entry *entry;

  if (equeue_empty(queue)) {
    event.handler = NULL;
    return (event);
  }

  if (queue->root) {
    entry = dequeue_child(queue);
  } else {
    entry = dequeue_child_from_root(queue);
  }

  event = entry->data.entry.event;

  FREE(entry);

  return (event);
}


static queue_entry *dequeue_child(equeue *queue)
{
  queue_entry *entry;

  entry = TAILQ_FIRST(&queue->head);
  TAILQ_REMOVE(&queue->head, entry, node);
  TAILQ_REMOVE(&queue->root->head, entry->data.entry.root, node);

  FREE(entry->data.entry.root);

  return (entry);
}


static queue_entry *dequeue_child_from_root(equeue *queue)
{
  queue_entry *entry, *centry;
  equeue *cqueue;

  entry = TAILQ_FIRST(&queue->head);
  TAILQ_REMOVE(&queue->head, entry, node);
  cqueue = entry->data.queue;
  centry = TAILQ_FIRST(&cqueue->head);
  TAILQ_REMOVE(&cqueue->head, centry, node);

  FREE(entry);

  return (centry);
}


void equeue_free(equeue *queue)
{
  queue_entry *entry;

  if (queue->root) {
    while (!equeue_empty(queue)) {
      entry = dequeue_child(queue);
      FREE(entry);
    }
  } else {
    while (!equeue_empty(queue)) {
      entry = dequeue_child_from_root(queue);
      FREE(entry);
    }
  }

  FREE(queue);
}


void equeue_run_events(equeue *queue)
{
  api_event event;

  while (!equeue_empty(queue)) {
    event = equeue_get(queue);

    if (event.handler)
      event.handler(&event.info);
  }
}
