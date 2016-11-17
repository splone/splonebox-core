
// Copyright (c) 2013, Ben Noordhuis <info@bnoordhuis.nl>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#pragma once

#include <stddef.h>

typedef struct _queue {
  struct _queue *next;
  struct _queue *prev;
} QUEUE;

// Public macros.
#define QUEUE_DATA(ptr, type, field) \
  ((type *)(void *)((char *)(ptr) - offsetof(type, field)))

#define QUEUE_FOREACH(q, h) \
  for (  /* NOLINT(readability/braces) */ \
      (q) = (h)->next; (q) != (h); (q) = (q)->next)

// ffi.cdef is unable to swallow `bool` in place of `int` here.
static inline int QUEUE_EMPTY(const QUEUE *const q)
{
  return q == q->next;
}

#define QUEUE_HEAD(q) (q)->next

static inline void QUEUE_INIT(QUEUE *const q)
{
  q->next = q;
  q->prev = q;
}

static inline void QUEUE_ADD(QUEUE *const h, QUEUE *const n)
{
  h->prev->next = n->next;
  n->next->prev = h->prev;
  h->prev = n->prev;
  h->prev->next = h;
}

static inline void QUEUE_SPLIT(QUEUE *const h, QUEUE *const q, QUEUE *const n)
{
  n->prev = h->prev;
  n->prev->next = n;
  n->next = q;
  h->prev = q->prev;
  h->prev->next = h;
  q->prev = n;
}

static inline void QUEUE_INSERT_HEAD(QUEUE *const h, QUEUE *const q)
{
  q->next = h->next;
  q->prev = h;
  q->next->prev = q;
  h->next = q;
}

static inline void QUEUE_INSERT_TAIL(QUEUE *const h, QUEUE *const q)
{
  q->next = h;
  q->prev = h->prev;
  q->prev->next = q;
  h->prev = q;
}

static inline void QUEUE_REMOVE(QUEUE *const q)
{
  q->prev->next = q->next;
  q->next->prev = q->prev;
}
