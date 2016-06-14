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

#include "sb-common.h"
#include "rpc/sb-rpc.h"

#define uint64_t_hash kh_int64_hash_func
#define uint64_t_eq kh_int64_hash_equal
#define uint32_t_hash kh_int_hash_func
#define uint32_t_eq kh_int_hash_equal

#define cstr_t_eq kh_str_hash_equal
#define cstr_t_hash kh_str_hash_func

#if defined(ARCH_64)
#define ptr_t_hash(key) uint64_t_hash((uint64_t)key)
#define ptr_t_eq(a, b) uint64_t_eq((uint64_t)a, (uint64_t)b)
#elif defined(ARCH_32)
#define ptr_t_hash(key) uint32_t_hash((uint32_t)key)
#define ptr_t_eq(a, b) uint32_t_eq((uint32_t)a, (uint32_t)b)
#endif

#define INITIALIZER(T, U) T##_##U##_initializer
#define INITIALIZER_DECLARE(T, U, ...) const U INITIALIZER(T, U) = __VA_ARGS__
#define DEFAULT_INITIALIZER {0}

#define MAP_IMPL(T, U, ...)                                                   \
  INITIALIZER_DECLARE(T, U, __VA_ARGS__);                                     \
  __KHASH_IMPL(T##_##U##_map,, T, U, 1, T##_hash, T##_eq)                     \
                                                                              \
  hashmap(T, U) *hashmap_##T##_##U##_new()                                    \
  {                                                                           \
    hashmap(T, U) *rv = MALLOC(hashmap(T, U));                                \
    rv->table = kh_init(T##_##U##_map);                                       \
    return rv;                                                                \
  }                                                                           \
                                                                              \
  void hashmap_##T##_##U##_free(hashmap(T, U) *map)                           \
  {                                                                           \
    kh_destroy(T##_##U##_map, map->table);                                    \
    FREE(map);                                                                \
  }                                                                           \
                                                                              \
  U hashmap_##T##_##U##_get(hashmap(T, U) *map, T key)                        \
  {                                                                           \
    khiter_t k;                                                               \
                                                                              \
    if ((k = kh_get(T##_##U##_map, map->table, key)) == kh_end(map->table)) { \
      return INITIALIZER(T, U);                                               \
    }                                                                         \
                                                                              \
    return kh_val(map->table, k);                                             \
  }                                                                           \
                                                                              \
  bool hashmap_##T##_##U##_has(hashmap(T, U) *map, T key)                     \
  {                                                                           \
    return kh_get(T##_##U##_map, map->table, key) != kh_end(map->table);      \
  }                                                                           \
                                                                              \
  U hashmap_##T##_##U##_put(hashmap(T, U) *map, T key, U value)               \
  {                                                                           \
    int ret;                                                                  \
    U rv = INITIALIZER(T, U);                                                 \
    khiter_t k = kh_put(T##_##U##_map, map->table, key, &ret);                \
                                                                              \
    if (!ret) {                                                               \
      rv = kh_val(map->table, k);                                             \
    }                                                                         \
                                                                              \
    kh_val(map->table, k) = value;                                            \
    return rv;                                                                \
  }                                                                           \
                                                                              \
  U *hashmap_##T##_##U##_ref(hashmap(T, U) *map, T key, bool put)             \
  {                                                                           \
    int ret;                                                                  \
    khiter_t k;                                                               \
    if (put) {                                                                \
      k = kh_put(T##_##U##_map, map->table, key, &ret);                       \
      if (ret) {                                                              \
        kh_val(map->table, k) = INITIALIZER(T, U);                            \
      }                                                                       \
    } else {                                                                  \
      k = kh_get(T##_##U##_map, map->table, key);                             \
      if (k == kh_end(map->table)) {                                          \
        return NULL;                                                          \
      }                                                                       \
    }                                                                         \
                                                                              \
    return &kh_val(map->table, k);                                            \
  }                                                                           \
                                                                              \
  U hashmap_##T##_##U##_del(hashmap(T, U) *map, T key)                        \
  {                                                                           \
    U rv = INITIALIZER(T, U);                                                 \
    khiter_t k;                                                               \
                                                                              \
    if ((k = kh_get(T##_##U##_map, map->table, key)) != kh_end(map->table)) { \
      rv = kh_val(map->table, k);                                             \
      kh_del(T##_##U##_map, map->table, k);                                   \
    }                                                                         \
                                                                              \
    return rv;                                                                \
  }                                                                           \
                                                                              \
  void hashmap_##T##_##U##_clear(hashmap(T, U) *map)                          \
  {                                                                           \
    kh_clear(T##_##U##_map, map->table);                                      \
  }

MAP_IMPL(string, ptr_t, DEFAULT_INITIALIZER)
MAP_IMPL(cstr_t, ptr_t, DEFAULT_INITIALIZER)
MAP_IMPL(string, dispatch_info, {.func = NULL, .async = false, .name = {.str = NULL, .length = 0}})
MAP_IMPL(uint64_t, string, {.str = NULL, .length = 0})
