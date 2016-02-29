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

/* verbosity global */
extern int8_t verbose_level;

/* uv loop global */
extern uv_loop_t loop;

/* Hashmap Functions */
/* Must be declared before initializing KHASH */

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static inline khint_t string_djb2_hash(string s)
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


/* Init khash */
KHASH_INIT(HASHMAP_STRING, string, void *, 1, string_djb2_hash, string_eq)

struct hashmap_string {
  khash_t(HASHMAP_STRING) *table;
};

/* Init khash */
KHASH_INIT(HASHMAP_UINT64, uint64_t, void *, 1, kh_int64_hash_func, kh_int64_hash_equal)

struct hashmap_uint64 {
  khash_t(HASHMAP_UINT64) *table;
};
/* Defines */

#define error_set(err, errtype, ...)                                \
  do {                                                              \
    if (snprintf((err)->msg, sizeof((err)->msg), __VA_ARGS__) < 0)  \
      abort();                                                      \
    (err)->isset = true;                                            \
    (err)->type = errtype;                                          \
  } while (0)

#define HASHMAP_ITERATE_VALUE(hashmap, var, code) \
  kh_foreach_value(hashmap->table, var, code)

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
    if (level >= verbose_level) {                     \
      LOG(__VA_ARGS__);                               \
    }                                                 \
  } while (0)

#define VERBOSE_OFF                 -1
#define VERBOSE_LEVEL_0             0

/* Functions */


/**
 * Creates a new `struct map` instance
 *
 * @return a pointer to the new instance
 */
struct hashmap_string *hashmap_string_new(void);

/**
 * Free the memory of the `struct hashmap_string` instance
 *
 * @param the `struct hashmap_string` instance to free
 */
void hashmap_string_free(struct hashmap_string *hmap);

/**
 * Returns the value to whiche the specified key is mapped
 *
 * @param hmap The `struct hashmap_string` instance
 * @param key The key whose associated value is to be returned
 * @return The value to which the specified key is mapped, or null if this map
 *         contains no mapping for the key
 */
void *hashmap_string_get(struct hashmap_string *hmap, string key);

/**
 * Returns true if this map contains a mapping for the specified key.
 *
 * @param hmap The `struct hashmap_string` instance
 * @param key The key whose presence in this map is to be tested
 * @return true if this map contains a mapping for the specified key, false
 *         otherwise
 */
bool hashmap_string_contains_key(struct hashmap_string *hmap, string key);

/**
 * Associates the specified value with the specified key in this map
 *
 * @param hmap The `struct hashmap_string` instance
 * @param key Key with which the specified value is to be associated
 * @param value Value to be associated with the specified key
 */
void *hashmap_string_put(struct hashmap_string *hmap, string key, void *value);

/**
 * Remove the mapping for a key from this map if it is present
 *
 * @param key key whose mapping is to be deleted from the map
 * @return The current value if exists or NULL otherwise
 */
void *hashmap_string_remove(struct hashmap_string *hmap, string key);

/**
 * Creates a new `struct map` instance
 *
 * @return a pointer to the new instance
 */
struct hashmap_uint64 *hashmap_uint64_new(void);

/**
 * Free the memory of the `struct hashmap` instance
 *
 * @param the `struct hashmap` instance to free
 */
void hashmap_uint64_free(struct hashmap_uint64 *hmap);

/**
 * Returns the value to whiche the specified key is mapped
 *
 * @param hmap The `struct hashmap` instance
 * @param key The key whose associated value is to be returned
 * @return The value to which the specified key is mapped, or null if this map
 *         contains no mapping for the key
 */
void *hashmap_uint64_get(struct hashmap_uint64 *hmap, uint64_t key);

/**
 * Returns true if this map contains a mapping for the specified key.
 *
 * @param hmap The `struct hashmap` instance
 * @param key The key whose presence in this map is to be tested
 * @return true if this map contains a mapping for the specified key, false
 *         otherwise
 */
bool hashmap_uint64_contains_key(struct hashmap_uint64 *hmap, uint64_t key);

/**
 * Associates the specified value with the specified key in this map
 *
 * @param hmap The `struct hashmap` instance
 * @param key Key with which the specified value is to be associated
 * @param value Value to be associated with the specified key
 */
void *hashmap_uint64_put(struct hashmap_uint64 *hmap, uint64_t key, void *value);

/**
 * Remove the mapping for a key from this map if it is present
 *
 * @param key key whose mapping is to be deleted from the map
 * @return The current value if exists or NULL otherwise
 */
void *hashmap_uint64_remove(struct hashmap_uint64 *hmap, uint64_t key);


void *reallocarray(void *optr, size_t nmemb, size_t size);

string cstring_to_string(char *str);
string cstring_copy_string(const char *str);
void free_string(string str);

int64_t randommod(long long n);

/**
 * The optparser parses the command line arguments. In case of an error,
 * it terminates the program (e.g. with exit(1)).
 * @param argc  argument count
 * @param argv  argument list
 */
void optparser(int argc, char **argv);
