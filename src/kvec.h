/* The MIT License

   Copyright (c) 2008, by Attractive Chaos <attractor@live.co.uk>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

/*
  An example:

#include "kvec.h"
int main() {
	kvec_t(int) array;
	kv_init(array);
	kv_push(int, array, 10); // append
	kv_a(int, array, 20) = 5; // dynamic
	kv_A(array, 20) = 4; // static
	kv_destroy(array);
	return 0;
}
*/

/*
  2008-09-22 (0.1.0):

	* The initial version.

*/

#ifndef AC_KVEC_H
#define AC_KVEC_H

#include <stdlib.h>
#include <string.h>

#include "sb-common.h"

#define kv_roundup32(x) \
    ((--(x)), \
     ((x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16), \
     (++(x)))

#define KV_INITIAL_VALUE { .size = 0, .capacity = 0, .items = NULL }

#define kvec_t(type) \
    struct { \
      size_t size; \
      size_t capacity; \
      type *items; \
    }

#define kv_init(v) ((v).size = (v).capacity = 0, (v).items = 0)
#define kv_destroy(v) free((v).items)
#define kv_A(v, i) ((v).items[(i)])
#define kv_pop(v) ((v).items[--(v).size])
#define kv_size(v) ((v).size)
#define kv_max(v) ((v).capacity)
#define kv_last(v) kv_A(v, kv_size(v) - 1)

#define kv_resize(v, s) \
    ((v).capacity = (s), \
     (v).items = realloc((v).items, sizeof((v).items[0]) * (v).capacity))

#define kv_resize_full(v) \
    kv_resize(v, (v).capacity ? (v).capacity << 1 : 8)

#define kv_copy(v1, v0) \
    do { \
      if ((v1).capacity < (v0).size) { \
        kv_resize(v1, (v0).size); \
      } \
      (v1).size = (v0).size; \
      memcpy((v1).items, (v0).items, sizeof((v1).items[0]) * (v0).size); \
    } while (0) \

#define kv_pushp(v) \
    ((((v).size == (v).capacity) ? (kv_resize_full(v), 0) : 0), \
     ((v).items + ((v).size++)))

#define kv_push(v, x) \
    (*kv_pushp(v) = (x))

#define kv_a(v, i) \
    (((v).capacity <= (size_t) (i) \
      ? ((v).capacity = (v).size = (i) + 1, \
         kv_roundup32((v).capacity), \
         kv_resize((v), (v).capacity), 0) \
      : ((v).size <= (size_t) (i) \
         ? (v).size = (i) + 1 \
         : 0)), \
     (v).items[(i)])

#define kvec_withinit_t(type, INIT_SIZE) \
    struct { \
      size_t size; \
      size_t capacity; \
      type *items; \
      type init_array[INIT_SIZE]; \
    }

#define kvi_init(v) \
    ((v).capacity = ARRAY_SIZE((v).init_array), \
     (v).size = 0, \
     (v).items = (v).init_array)

/// Move data to a new destination and free source
static inline void *_memcpy_free(void *const restrict dest,
                                 void *const restrict src,
                                 const size_t size)
{
  memcpy(dest, src, size);
  free(src);
  return dest;
}


#define kvi_resize(v, s) \
    ((v).capacity = ((s) > ARRAY_SIZE((v).init_array) \
                     ? (s) \
                     : ARRAY_SIZE((v).init_array)), \
     (v).items = ((v).capacity == ARRAY_SIZE((v).init_array) \
                  ? ((v).items == (v).init_array \
                     ? (v).items \
                     : _memcpy_free((v).init_array, (v).items, \
                                    (v).size * sizeof((v).items[0]))) \
                  : ((v).items == (v).init_array \
                     ? memcpy(xmalloc((v).capacity * sizeof((v).items[0])), \
                              (v).items, \
                              (v).size * sizeof((v).items[0])) \
                     : realloc((v).items, \
                                (v).capacity * sizeof((v).items[0])))))

/// Resize vector with preallocated array when it is full
///
/// @param[out]  v  Vector to resize.
#define kvi_resize_full(v) \
    /* ARRAY_SIZE((v).init_array) is the minimal capacity of this vector. */ \
    /* Thus when vector is full capacity may not be zero and it is safe */ \
    /* not to bother with checking whether (v).capacity is 0. But now */ \
    /* capacity is not guaranteed to have size that is a power of 2, it is */ \
    /* hard to fix this here and is not very necessary if users will use */ \
    /* 2^x initial array size. */ \
    kvi_resize(v, (v).capacity << 1)

#define kvi_pushp(v) \
    ((((v).size == (v).capacity) ? (kvi_resize_full(v), 0) : 0), \
     ((v).items + ((v).size++)))

#define kvi_push(v, x) \
    (*kvi_pushp(v) = (x))

#define kvi_destroy(v) \
    do { \
      if ((v).items != (v).init_array) { \
        FREE((v).items); \
      } \
    } while (0)

#endif
