#pragma once

#include "sb-common.h"

#define OBJECT_INIT { .type = OBJECT_TYPE_NIL }

#define OBJECT_OBJ(o) o

#define BOOLEAN_OBJ(b) ((object) { \
  .type = OBJECT_TYPE_BOOL, \
  .data.boolean = b })

#define INTEGER_OBJ(i) ((object) { \
  .type = OBJECT_TYPE_INT, \
  .data.integer = i })

#define UINTEGER_OBJ(i) ((object) { \
  .type = OBJECT_TYPE_UINT, \
  .data.uinteger = i })

#define FLOATING_OBJ(f) ((object) { \
  .type = OBJECT_TYPE_FLOAT, \
  .data.floating = f })

#define STRING_OBJ(s) ((object) { \
  .type = OBJECT_TYPE_STR, \
  .data.string = s })

#define ARRAY_OBJ(a) ((object) { \
  .type = OBJECT_TYPE_ARRAY, \
  .data.array = a })

#define DICTIONARY_OBJ(d) ((object) { \
  .type = OBJECT_TYPE_DICTIONARY, \
  .data.dictionary = d })

#define NIL ((object) {.type = OBJECT_TYPE_NIL})

#define PUT(dict, k, v) \
  kv_push(dict, ((key_value_pair) { .key = cstring_copy_string(k), .value = v }))

#define ADD(array, item) \
  kv_push(array, item)

#define STATIC_CSTR_AS_STRING(s) ((string) {.str = s, .length = sizeof(s) - 1})
