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

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <uv.h>

#include "queue.h"
#include "khash.h"
#include "kvec.h"

/* Structs */
#define API_ERROR_MESSAGE_LEN 512

typedef enum {
  API_ERROR_TYPE_VALIDATION,
} api_error_type;

typedef struct {
  char *str;
  size_t length;
} string;

struct api_error {
  api_error_type type;
  char msg[API_ERROR_MESSAGE_LEN];
  bool isset;
};

typedef void * ptr_t;

/* verbosity global */
extern int8_t verbose_level;

/* uv loop global */
extern uv_loop_t loop;

/* Hashmap Functions */
/* Must be declared before initializing KHASH */

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static inline khint_t string_hash(string s)
{
  khint_t h = 5831;

  for (size_t i = 0; i < s.length && s.str[i]; i++)
    h = ((h << 5) + h) + (uint8_t)s.str[i];

  return (h);
}


static inline bool string_eq(string a, string b)
{
  return (strncmp(a.str, b.str, MIN(a.length, b.length)) == 0);
}

/* Defines */
#define hashmap(T, U) hashmap_##T##_##U

#define MAP_DECLS(T, U)                                                       \
  KHASH_DECLARE(T##_##U##_map, T, U)                                          \
                                                                              \
  typedef struct {                                                            \
    khash_t(T##_##U##_map) *table;                                            \
  } hashmap(T, U);                                                            \
                                                                              \
  hashmap(T, U) *hashmap_##T##_##U##_new(void);                               \
  void hashmap_##T##_##U##_free(hashmap(T, U) *map);                          \
  U hashmap_##T##_##U##_get(hashmap(T, U) *map, T key);                       \
  bool hashmap_##T##_##U##_has(hashmap(T, U) *map, T key);                    \
  U hashmap_##T##_##U##_put(hashmap(T, U) *map, T key, U value);              \
  U *hashmap_##T##_##U##_ref(hashmap(T, U) *map, T key, bool put);            \
  U hashmap_##T##_##U##_del(hashmap(T, U) *map, T key);                       \
  void hashmap_##T##_##U##_clear(hashmap(T, U) *map);

#define hashmap_new(T, U) hashmap_##T##_##U##_new
#define hashmap_free(T, U) hashmap_##T##_##U##_free
#define hashmap_get(T, U) hashmap_##T##_##U##_get
#define hashmap_has(T, U) hashmap_##T##_##U##_has
#define hashmap_put(T, U) hashmap_##T##_##U##_put
#define hashmap_ref(T, U) hashmap_##T##_##U##_ref
#define hashmap_del(T, U) hashmap_##T##_##U##_del
#define hashmap_clear(T, U) hashmap_##T##_##U##_clear

#define error_set(err, errtype, ...)                                \
  do {                                                              \
    if (snprintf((err)->msg, sizeof((err)->msg), __VA_ARGS__) < 0)  \
      abort();                                                      \
    (err)->isset = true;                                            \
    (err)->type = errtype;                                          \
  } while (0)

#define hashmap_foreach_value(map, value, block) \
  kh_foreach_value(map->table, value, block)

#if defined(__clang__) ||                       \
  defined(__GNUC__) ||                          \
  defined(__INTEL_COMPILER) ||                  \
  defined(__SUNPRO_C)
#define UNUSED(x) __attribute__((unused)) x
#else
define UNUSED(x) x
#endif

#define MALLOC(type) ((type *)reallocarray(NULL, 1, sizeof(type)))

#define MALLOC_ARRAY(number, type)                    \
  ((type *)reallocarray(NULL, number, sizeof(type)))

#define CALLOC(number, type)                    \
  ((type *)calloc(number, sizeof(type)))

#define REALLOC_ARRAY(pointer, number, type)              \
  ((type *)reallocarray(pointer, number,  sizeof(type)))

#define FREE(pointer) do {                      \
    free(pointer);                              \
    pointer = NULL;                             \
  } while (0)

#define LOG(...)                  \
  do {                            \
    fprintf(stdout, __VA_ARGS__); \
    fflush(stdout);               \
  } while(0)                      \

#define LOG_ERROR(...)            \
  do {                            \
    errx(1, __VA_ARGS__);         \
  } while(0)

#define LOG_WARNING(...)          \
  do {                            \
    warnx(__VA_ARGS__);           \
  } while(0)

#define LOG_VERBOSE(level, ...)                       \
  do {                                                \
    if (level <= verbose_level) {                     \
      LOG(__VA_ARGS__);                               \
    }                                                 \
  } while (0)

#define VERBOSE_OFF                 -1
#define VERBOSE_LEVEL_0             0
#define VERBOSE_LEVEL_1             1
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* Functions */
void *reallocarray(void *optr, size_t nmemb, size_t size);

string cstring_to_string(char *str);
string cstring_copy_string(const char *str);
void free_string(string str);
void sbmemzero(void * const pnt, const size_t len);

int64_t randommod(long long n);

/**
 * The optparser parses the command line arguments. In case of an error,
 * it terminates the program (e.g. with exit(1)).
 * @param argc  argument count
 * @param argv  argument list
 */
void optparser(int argc, char **argv);

int signal_init(void);

int filesystem_open_write(const char *fn);
int filesystem_open_read(const char *fn);
int filesystem_write_all(int fd, const void *x, size_t xlen);
int filesystem_read_all(int fd, void *x, size_t xlen);
int filesystem_save_sync(const char *fn, const void *x, size_t xlen);
int filesystem_load(const char *fn, void *x, size_t xlen);
