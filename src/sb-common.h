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
 *
 *    and
 *
 *    Copyright (c) 2001-2004, Roger Dingledine
 *    Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson
 *    Copyright (c) 2007-2016, The Tor Project, Inc.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are
 *    met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following disclaimer
 *        in the documentation and/or other materials provided with the
 *        distribution.
 *
 *      * Neither the names of the copyright owners nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

typedef enum {
  SERVER_TYPE_TCP,
  SERVER_TYPE_PIPE,
  SERVER_TYPE_UNKNOWN
} server_type;

struct api_error {
  api_error_type type;
  char msg[API_ERROR_MESSAGE_LEN];
  bool isset;
};

/** A linked list of lines in a config file. */
typedef struct configline {
  char *key;
  char *value;
  struct configline *next;
  /** What special treatment (if any) does this line require? */
  unsigned int command:2;
  /** If true, subsequent assignments to this linelist should replace
   * it, not extend it.  Set only on the first item in a linelist in an
   * options. */
  unsigned int fragile:1;
} configline;

/** Enumeration of types which option values can take */
typedef enum configtype {
  CONFIG_TYPE_STRING = 0,   /**< An arbitrary string. */
  CONFIG_TYPE_FILENAME,     /**< A filename: some prefixes get expanded. */
  CONFIG_TYPE_UINT,         /**< A non-negative integer less than MAX_INT */
  CONFIG_TYPE_INT,          /**< Any integer. */
  CONFIG_TYPE_PORT,         /**< A port from 1...65535, 0 for "not set", or
                             * "auto".  */
  CONFIG_TYPE_INTERVAL,     /**< A number of seconds, with optional units*/
  CONFIG_TYPE_MSEC_INTERVAL,/**< A number of milliseconds, with optional
                              * units */
  CONFIG_TYPE_MEMUNIT,      /**< A number of bytes, with optional units*/
  CONFIG_TYPE_DOUBLE,       /**< A floating-point value */
  CONFIG_TYPE_BOOL,         /**< A boolean value, expressed as 0 or 1. */
  CONFIG_TYPE_AUTOBOOL,     /**< A boolean+auto value, expressed 0 for false,
                             * 1 for true, and -1 for auto  */
  CONFIG_TYPE_ISOTIME,      /**< An ISO-formatted time relative to UTC. */
  CONFIG_TYPE_LINELIST,     /**< Uninterpreted config lines */
  CONFIG_TYPE_LINELIST_S,   /**< Uninterpreted, context-sensitive config lines,
                             * mixed with other keywords. */
  CONFIG_TYPE_LINELIST_V,   /**< Catch-all "virtual" option to summarize
                             * context-sensitive config lines when fetching.
                             */
  CONFIG_TYPE_OBSOLETE,     /**< Obsolete (ignored) option. */
} configtype;

/** Type of a callback to validate whether a given configuration is
 * well-formed and consistent. See options_trial_assign() for documentation
 * of arguments. */
typedef int (*validatefn)(void*, int);

/** An abbreviation for a configuration option allowed on the command line. */
typedef struct configabbrev {
  const char *abbreviated;
  const char *full;
  int commandline_only;
  int warn;
} configabbrev;

/** A variable allowed in the configuration file or on the command line. */
typedef struct configvar {
  const char *name; /**< The full keyword (case insensitive). */
  configtype type; /**< How to interpret the type and turn it into a
                       * value. */
  off_t var_offset; /**< Offset of the corresponding member of options. */
  const char *initvalue; /**< String (or null) describing initial value. */
} configvar;

/** Information on the keys, value types, key-to-struct-member mappings,
 * variable descriptions, validation functions, and abbreviations for a
 * configuration or storage format. */
typedef struct configformat {
  size_t size; /**< Size of the struct that everything gets parsed into. */
  uint32_t magic; /**< Required 'magic value' to make sure we have a struct
                   * of the right type. */
  off_t magic_offset; /**< Offset of the magic value within the struct. */
  configabbrev *abbrevs; /**< List of abbreviations that we expand when
                             * parsing this format. */
  configvar *vars; /**< List of variables we recognize, their default
                       * values, and where we stick them in the structure. */
  configvar *extra;
} configformat;

/** Holds an IPv4 or IPv6 address.  (Uses less memory than struct
 * sockaddr_storage.) */
typedef struct boxaddr
{
  sa_family_t family;
  union {
    uint32_t dummy_; /* This field is here so we have something to initialize
                      * with a reliable cross-platform type. */
    struct in_addr in_addr;
    struct in6_addr in6_addr;
  } addr;
} boxaddr;

/** Configuration options for a splonebox process. */
typedef struct {
  uint32_t magic_;

  char *ApiTransportListen;
  boxaddr ApiTransportListenAddr;
  uint16_t ApiTransportListenPort;

  char *RedisDatabaseListen;
  boxaddr RedisDatabaseListenAddr;
  uint16_t RedisDatabaseListenPort;
  char *RedisDatabaseAuth;

  char *ApiNamedPipeListen;
  server_type apitype;

  char *ContactInfo;
  /** Ports to listen on for SOCKS connections. */
  uint16_t RedisPort;
} options;

/** An error from options_trial_assign() or options_init_from_string(). */
typedef enum setoptionerror {
  SETOPT_OK = 0,
  SETOPT_ERR_MISC = -1,
  SETOPT_ERR_PARSE = -2,
  setoptionerrorRANSITION = -3,
  SETOPT_ERR_SETTING = -4,
} setoptionerror;

/** Mapping from a unit name to a multiplier for converting that unit into a
     * base unit.  Used by config_parse_unit. */
struct unit_table_t {
  const char *unit; /**< The name of the unit */
  uint64_t multiplier; /**< How many of the base unit appear in this unit */
};

typedef void * ptr_t;
typedef const char * cstr_t;

/* verbosity global */
extern int8_t verbose_level;

/* uv loop global */
extern uv_loop_t loop;

/* address parsing helper inline functions */
/** Helper: given a hex digit, return its value, or -1 if it isn't hex. */
static inline int hex_decode_digit(char c)
{
  switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'A': case 'a': return 10;
    case 'B': case 'b': return 11;
    case 'C': case 'c': return 12;
    case 'D': case 'd': return 13;
    case 'E': case 'e': return 14;
    case 'F': case 'f': return 15;
    default:
      return -1;
  }
}

extern const uint32_t ISDIGIT_TABLE[8];
extern const uint32_t ISSPACE_TABLE[8];
extern const uint32_t ISXDIGIT_TABLE[8];

static inline int ISDIGIT(char c) {
    uint8_t u = (uint8_t) c;
    return !!(ISDIGIT_TABLE[(u >> 5) & 7] & (1u << (u & 31)));
}

static inline int ISSPACE(char c) {
    uint8_t u = (uint8_t) c;
    return !!(ISSPACE_TABLE[(u >> 5) & 7] & (1u << (u & 31)));
}

static inline int ISXDIGIT(char c) {
    uint8_t u = (uint8_t) c;
    return !!(ISXDIGIT_TABLE[(u >> 5) & 7] & (1u << (u & 31)));
}

#define ISODIGIT(c) ('0' <= (c) && (c) <= '7')

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

#if 8 == 8
#define ARCH_64
#elif 8 == 4
#define ARCH_32
#endif

#ifdef BOX_UNIT_TESTS
#define STATIC
#else
#define STATIC static
#endif

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

#define BOX_ADDR_BUF_LEN 48

#define ARRAY_INIT {.size = 0, .capacity = 0, .obj = NULL}
#define STRING_INIT {.str = NULL, .length = 0}
#define ERROR_INIT {.isset = false}

/* Functions */
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

int signal_init(void);

int filesystem_open_write(const char *fn);
int filesystem_open_read(const char *fn);
int filesystem_write_all(int fd, const void *x, size_t xlen);
int filesystem_read_all(int fd, void *x, size_t xlen);
int filesystem_save_sync(const char *fn, const void *x, size_t xlen);
int filesystem_load(const char *fn, void *x, size_t xlen);

int options_init_from_boxrc(void);
options * options_get(void);
void options_free(options *options);

/** If <b>key</b> is a configuration option, return the corresponding const
 * configvar.  Otherwise, if <b>key</b> is a non-standard abbreviation,
 * warn, and return the corresponding const configvar.  Otherwise return
 * NULL.
 */
const configvar * confparse_find_option(const configformat *fmt,
    const char *key);

/**
 * Free all the configuration lines on the linked list <b>front</b>.
 */
void confparse_free_lines(configline *front);
const char * confparse_line_from_str_verbose(const char *line,
    char **key_out,char **value_out, const char **err_out);
void confparse_free(const configformat *fmt, void *options);
void confparse_init(const configformat *fmt, void *options);
int confparse_get_lines(const char *string, configline **result, int extended);

/** If <b>option</b> is an official abbreviation for a longer option,
 * return the longer option.  Otherwise return <b>option</b>.
 * If <b>command_line</b> is set, apply all abbreviations.  Otherwise, only
 * apply abbreviations that work for the config file and the command line.
 * If <b>warn_obsolete</b> is set, warn about deprecated names. */
const char * confparse_expand_abbrev(const configformat *fmt,
    const char *option, int command_line, int warn_obsolete);
int confparse_assign(const configformat *fmt, void *options,
    configline *list, int use_defaults, int clear_first);

long parse_long(const char *s, int base, long min, long max, int *ok,
    char **next);
int parse_iso_time(const char *cp, time_t *t);
int parse_msec_interval(const char *s, int *ok);
int parse_timegm(const struct tm *tm, time_t *time_out);
int parse_interval(const char *s, int *ok);
uint64_t parse_memunit(const char *s, int *ok);
uint64_t parse_units(const char *val, struct unit_table_t *u, int *ok);
const char * eat_whitespace(const char *s);
char * box_strdup(const char *s);
char * box_strndup(const char *s, size_t n);
int box_sscanf(const char *buf, const char *pattern, ...);
int box_vsscanf(const char *buf, const char *pattern, va_list ap);

int box_addr_port_lookup(const char *s, boxaddr *addr_out, uint16_t *port_out);
socklen_t box_addr_to_sockaddr(const boxaddr *a, uint16_t port,
    struct sockaddr *sa_out, socklen_t len);
const char * fmt_addr(const boxaddr *addr);
const char * box_addr_to_str(char *dest, const boxaddr *addr, socklen_t len);
