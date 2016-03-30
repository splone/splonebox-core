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
 *
 *    This file incorporates code covered by the following terms:
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

#include <assert.h>
#include <ctype.h>
#include "sb-common.h"
#include "confparse.h"

#define STRUCT_VAR_P(st, off) ((void*) ( ((char*)(st)) + (off) ) )

#if defined(ARCH_32)
#define BITARRAY_SHIFT 5
#elif defined(ARCH_64)
#define BITARRAY_SHIFT 6
#else
#error "int is neither 4 nor 8 bytes. I can't deal with that."
#endif
#define BITARRAY_MASK ((1u<<BITARRAY_SHIFT)-1)

/** Ordinary configuration line. */
#define CONFIG_LINE_NORMAL 0
/** Appends to previous configuration for the same option, even if we
 * would ordinary replace it. */
#define CONFIG_LINE_APPEND 1
/* Removes all previous configuration for an option. */
#define CONFIG_LINE_CLEAR 2


const uint32_t ISDIGIT_TABLE[8] = { 0, 0x3ff0000, 0, 0, 0, 0, 0, 0 };
const uint32_t ISSPACE_TABLE[8] = { 0x3e00, 0x1, 0, 0, 0, 0, 0, 0 };
const uint32_t ISXDIGIT_TABLE[8] = { 0, 0x3ff0000, 0x7e, 0x7e, 0, 0, 0, 0 };

/** Create a new bit array that can hold <b>n_bits</b> bits. */
static inline bitarray_t *
bitarray_init_zero(unsigned int n_bits)
{
  /* round up to the next int. */
  size_t sz = (n_bits+BITARRAY_MASK) >> BITARRAY_SHIFT;
  return (CALLOC(sz, unsigned int));
}

/** Free the bit array <b>ba</b>. */
static inline void
bitarray_free(bitarray_t *ba)
{
  FREE(ba);
}

/** Return true iff <b>bit</b>th bit in <b>b</b> is nonzero.  NOTE: does
 * not necessarily return 1 on true. */
static inline unsigned int
bitarray_is_set(bitarray_t *b, int bit)
{
  return (b[bit >> BITARRAY_SHIFT] & (1u << ((unsigned int)bit & BITARRAY_MASK)));
}

/** Set the <b>bit</b>th bit in <b>b</b> to 1. */
static inline void
bitarray_set(bitarray_t *b, int bit)
{
  b[bit >> BITARRAY_SHIFT] |= (1u << ((unsigned int)bit & BITARRAY_MASK));
}

/** Return the number of option entries in <b>fmt</b>. */
STATIC int config_count_options(const configformat *fmt)
{
  int i;

  for (i=0; fmt->vars[i].name; ++i)
    ;

  return (i);
}

/** Mark every linelist in <b>options</b> "fragile", so that fresh assignments
 * to it will replace old ones. */
STATIC void mark_lists_fragile(const configformat *fmt, void *options)
{
  int i;

  assert(fmt);
  assert(options);

  for (i = 0; fmt->vars[i].name; ++i) {
    const configvar *var = &fmt->vars[i];
    configline *list;
    if (var->type != CONFIG_TYPE_LINELIST &&
        var->type != CONFIG_TYPE_LINELIST_V)
      continue;

    list = *(configline **)STRUCT_VAR_P(options, var->var_offset);
    if (list)
      list->fragile = 1;
  }
}

STATIC void reset_line(const configformat *fmt, void *options, const char *key,
    int use_defaults)
{
  const configvar *var;

  assert(fmt && options);
  assert((fmt)->magic == *(uint32_t*)STRUCT_VAR_P(options,fmt->magic_offset));

  var = confparse_find_option(fmt, key);
  if (!var)
    return; /* give error on next pass. */

  reset(fmt, options, var, use_defaults);
}

/** If <b>c</b> is a syntactically valid configuration line, update
 * <b>options</b> with its value and return 0.  Otherwise return -1 for bad
 * key, -2 for bad value.
 *
 * If <b>clear_first</b> is set, clear the value first. Then if
 * <b>use_defaults</b> is set, set the value to the default.
 *
 * Called from confparse_assign().
 */
STATIC int assign_line(const configformat *fmt, void *options, configline *c,
    int use_defaults, int clear_first, bitarray_t *options_seen)
{
  const configvar *var;

  assert(fmt && options);
  assert((fmt)->magic == *(uint32_t*)STRUCT_VAR_P(options,fmt->magic_offset));

  var = confparse_find_option(fmt, c->key);
  if (!var) {
    if (fmt->extra) {
      void *lvalue = STRUCT_VAR_P(options, fmt->extra->var_offset);
      LOG_VERBOSE(VERBOSE_LEVEL_0, "Found unrecognized option '%s'; saving it.",
          c->key);
      line_append((configline**)lvalue, c->key, c->value);
      return (0);
    } else {
      LOG_WARNING("Unknown option '%s'.  Failing.", c->key);
      return (-1);
    }
  }

  /* Put keyword into canonical case. */
  if (strcmp(var->name, c->key)) {
    FREE(c->key);
    c->key = box_strdup(var->name);
  }

  if (!strlen(c->value)) {
    /* reset or clear it, then return */
    if (!clear_first) {
      if ((var->type == CONFIG_TYPE_LINELIST ||
           var->type == CONFIG_TYPE_LINELIST_S) &&
          c->command != CONFIG_LINE_CLEAR) {
        /* We got an empty linelist from the torrc or command line.
           As a special case, call this an error. Warn and ignore. */
        LOG_WARNING("Linelist option '%s' has no value. Skipping.", c->key);
      } else { /* not already cleared */
        reset(fmt, options, var, use_defaults);
      }
    }
    return (0);
  } else if (c->command == CONFIG_LINE_CLEAR && !clear_first) {
    reset(fmt, options, var, use_defaults);
  }

  if (options_seen && (var->type != CONFIG_TYPE_LINELIST &&
                       var->type != CONFIG_TYPE_LINELIST_S)) {
    /* We're tracking which options we've seen, and this option is not
     * supposed to occur more than once. */
    int var_index = (int)(var - fmt->vars);
    if (bitarray_is_set(options_seen, var_index)) {
      LOG_WARNING("Option '%s' used more than once; all but the last "
               "value will be ignored.", var->name);
    }
    bitarray_set(options_seen, var_index);
  }

  if (assign_value(fmt, options, c) < 0)
    return -2;

  return (0);
}

/** Clear the option indexed by <b>var</b> in <b>options</b>. Then if
 * <b>use_defaults</b>, set it to its default value.
 * Called by confparse_init() and option_reset_line() and option_assign_line(). */
STATIC void reset(const configformat *fmt, void *options, const configvar *var,
    int use_defaults)
{
  configline *c;

  assert(fmt && options);
  assert((fmt)->magic == *(uint32_t*)STRUCT_VAR_P(options,fmt->magic_offset));
  clear(fmt, options, var); /* clear it first */

  if (!use_defaults)
    return; /* all done */
  if (var->initvalue) {
    c = CALLOC(1, configline);
    c->key = box_strdup(var->name);
    c->value = box_strdup(var->initvalue);
    if (assign_value(fmt, options, c) < 0) {
      LOG_WARNING("Failed to assign default");
    }
    confparse_free_lines(c);
  }
}

/** Reset config option <b>var</b> to 0, 0.0, NULL, or the equivalent.
 * Called from reset() and confparse_free(). */
 /** Reset config option <b>var</b> to 0, 0.0, NULL, or the equivalent.
  * Called from reset() and confparse_free(). */
STATIC void clear(const configformat *fmt, void *options, const configvar *var)
{
  void *lvalue = STRUCT_VAR_P(options, var->var_offset);
  (void)fmt; /* unused */
  switch (var->type) {
    case CONFIG_TYPE_STRING:
    case CONFIG_TYPE_FILENAME:
      FREE(*(char**)lvalue);
      break;
    case CONFIG_TYPE_DOUBLE:
      *(double*)lvalue = 0.0;
      break;
    case CONFIG_TYPE_ISOTIME:
      *(time_t*)lvalue = 0;
      break;
    case CONFIG_TYPE_INTERVAL:
    case CONFIG_TYPE_MSEC_INTERVAL:
    case CONFIG_TYPE_UINT:
    case CONFIG_TYPE_INT:
    case CONFIG_TYPE_PORT:
    case CONFIG_TYPE_BOOL:
      *(int*)lvalue = 0;
      break;
    case CONFIG_TYPE_AUTOBOOL:
      *(int*)lvalue = -1;
      break;
    case CONFIG_TYPE_MEMUNIT:
      *(uint64_t*)lvalue = 0;
      break;
    case CONFIG_TYPE_LINELIST:
    case CONFIG_TYPE_LINELIST_S:
      confparse_free_lines(*(configline **)lvalue);
      *(configline **)lvalue = NULL;
      break;
    case CONFIG_TYPE_LINELIST_V:
      /* handled by linelist_s. */
      break;
    case CONFIG_TYPE_OBSOLETE:
      break;
  }
}

STATIC const char * unescape_string(const char *s, char **result,
    size_t *size_out)
{
  const char *cp;
  char *out;

  if (s[0] != '\"')
    return (NULL);

  cp = s+1;
  while (1) {
    switch (*cp) {
      case '\0':
      case '\n':
        return (NULL);
      case '\"':
        goto end_of_loop;
      case '\\':
        if (cp[1] == 'x' || cp[1] == 'X') {
          if (!(ISXDIGIT(cp[2]) && ISXDIGIT(cp[3])))
            return (NULL);
          cp += 4;
        } else if (ISODIGIT(cp[1])) {
          cp += 2;
          if (ISODIGIT(*cp)) ++cp;
          if (ISODIGIT(*cp)) ++cp;
        } else if (cp[1] == 'n' || cp[1] == 'r' || cp[1] == 't' || cp[1] == '"'
                   || cp[1] == '\\' || cp[1] == '\'') {
          cp += 2;
        } else {
          return (NULL);
        }
        break;
      default:
        ++cp;
        break;
    }
  }
end_of_loop:
  out = *result = MALLOC_ARRAY((size_t)(cp-s + 1), char);
  cp = s+1;
  while (1) {
    switch (*cp)
      {
      case '\"':
        *out = '\0';
        if (size_out) *size_out =  (size_t)(out - *result);
        return cp+1;
      case '\0':
        FREE(*result);
        return (NULL);
      case '\\':
        switch (cp[1])
          {
          case 'n': *out++ = '\n'; cp += 2; break;
          case 'r': *out++ = '\r'; cp += 2; break;
          case 't': *out++ = '\t'; cp += 2; break;
          case 'x': case 'X':
            {
              int x1, x2;

              x1 = hex_decode_digit(cp[2]);
              x2 = hex_decode_digit(cp[3]);
              if (x1 == -1 || x2 == -1) {
                  FREE(*result);
                  return (NULL);
              }

              *out++ = (char)((x1<<4) + x2);
              cp += 4;
            }
            break;
          case '0': case '1': case '2': case '3': case '4': case '5':
          case '6': case '7':
            {
              int n = cp[1]-'0';
              cp += 2;
              if (ISODIGIT(*cp)) { n = n*8 + *cp-'0'; cp++; }
              if (ISODIGIT(*cp)) { n = n*8 + *cp-'0'; cp++; }
              if (n > 255) { FREE(*result); return (NULL); }
              *out++ = (char)n;
            }
            break;
          case '\'':
          case '\"':
          case '\\':
          case '\?':
            *out++ = cp[1];
            cp += 2;
            break;
          default:
            FREE(*result); return (NULL);
          }
        break;
      default:
        *out++ = *cp++;
      }
  }
}

/** Helper: allocate a new configuration option mapping 'key' to 'val',
 * append it to *<b>lst</b>. */
STATIC void line_append(configline **lst, const char *key, const char *val)
{
  configline *newline;

  newline = CALLOC(1, configline);
  newline->key = box_strdup(key);
  newline->value = box_strdup(val);
  newline->next = NULL;

  while (*lst)
    lst = &((*lst)->next);

  (*lst) = newline;
}

/** <b>c</b>-\>key is known to be a real key. Update <b>options</b>
 * with <b>c</b>-\>value and return 0, or return -1 if bad value.
 *
 * Called from assign_line() and option_reset().
 */
STATIC int assign_value(const configformat *fmt, void *options, configline *c)
{
  int i, ok;
  const configvar *var;
  void *lvalue;

  assert(fmt && options);
  assert((fmt)->magic == *(uint32_t*)STRUCT_VAR_P(options,fmt->magic_offset));

  var = confparse_find_option(fmt, c->key);
  assert(var);

  lvalue = STRUCT_VAR_P(options, var->var_offset);

  switch (var->type) {

  case CONFIG_TYPE_PORT:
    /* fall through */
  case CONFIG_TYPE_INT:
  case CONFIG_TYPE_UINT:
    i = (int)parse_long(c->value, 10,
                            var->type==CONFIG_TYPE_INT ? INT_MIN : 0,
                            var->type==CONFIG_TYPE_PORT ? 65535 : INT_MAX,
                            &ok, NULL);
    if (!ok) {
      LOG_WARNING("Int keyword '%s %s' is malformed or out of bounds.",
          c->key, c->value);
      return (-1);
    }
    *(int *)lvalue = i;
    break;

  case CONFIG_TYPE_INTERVAL: {
    i = parse_interval(c->value, &ok);
    if (!ok) {
      LOG_WARNING("Interval '%s %s' is malformed or out of bounds.", c->key,
          c->value);
      return (-1);
    }
    *(int *)lvalue = i;
    break;
  }

  case CONFIG_TYPE_MSEC_INTERVAL: {
    i = parse_msec_interval(c->value, &ok);
    if (!ok) {
      LOG_WARNING("Msec interval '%s %s' is malformed or out of bounds.",
          c->key, c->value);
      return (-1);
    }
    *(int *)lvalue = i;
    break;
  }

  case CONFIG_TYPE_MEMUNIT: {
    uint64_t u64 = parse_memunit(c->value, &ok);
    if (!ok) {
      LOG_WARNING("Value '%s %s' is malformed or out of bounds.", c->key,
          c->value);
      return (-1);
    }
    *(uint64_t *)lvalue = u64;
    break;
  }

  case CONFIG_TYPE_BOOL:
    i = (int)parse_long(c->value, 10, 0, 1, &ok, NULL);
    if (!ok) {
      LOG_WARNING("Boolean '%s %s' expects 0 or 1.", c->key, c->value);
      return (-1);
    }
    *(int *)lvalue = i;
    break;

  case CONFIG_TYPE_AUTOBOOL:
    if (!strcmp(c->value, "auto"))
      *(int *)lvalue = -1;
    else if (!strcmp(c->value, "0"))
      *(int *)lvalue = 0;
    else if (!strcmp(c->value, "1"))
      *(int *)lvalue = 1;
    else {
      LOG_WARNING("Boolean '%s %s' expects 0, 1, or 'auto'.", c->key, c->value);
      return (-1);
    }
    break;

  case CONFIG_TYPE_STRING:
  case CONFIG_TYPE_FILENAME:
    FREE(*(char **)lvalue);
    *(char **)lvalue = box_strdup(c->value);
    break;

  case CONFIG_TYPE_DOUBLE:
    *(double *)lvalue = atof(c->value);
    break;

  case CONFIG_TYPE_ISOTIME:
    if (parse_iso_time(c->value, (time_t *)lvalue)) {
      LOG_WARNING("Invalid time '%s' for keyword '%s'", c->value, c->key);
      return (-1);
    }
    break;

  case CONFIG_TYPE_LINELIST:
  case CONFIG_TYPE_LINELIST_S:
    {
      configline *lastval = *(configline**)lvalue;
      if (lastval && lastval->fragile) {
        if (c->command != CONFIG_LINE_APPEND) {
          confparse_free_lines(lastval);
          *(configline**)lvalue = NULL;
        } else {
          lastval->fragile = 0;
        }
      }

      line_append((configline**)lvalue, c->key, c->value);
    }
    break;
  case CONFIG_TYPE_OBSOLETE:
    LOG_WARNING("Skipping obsolete configuration option '%s'", c->key);
    break;
  case CONFIG_TYPE_LINELIST_V:
    LOG_WARNING("You may not provide a value for virtual option '%s'", c->key);
    return (-1);
  default:
    assert(0);
    break;
  }

  return (0);
}


/** As confparse_find_option, but return a non-const pointer. */
STATIC configvar * find_option_mutable(const configformat *fmt,
    const char *key)
{
  int i;
  size_t keylen = strlen(key);

  if (!keylen)
    return (NULL); /* if they say "--" on the command line, it's not an option */

  /* First, check for an exact (case-insensitive) match */
  for (i=0; fmt->vars[i].name; ++i) {
    if (!strcasecmp(key, fmt->vars[i].name)) {
      return (&fmt->vars[i]);
    }
  }

  /* If none, check for an abbreviated match */
  for (i=0; fmt->vars[i].name; ++i) {
    if (!strncasecmp(key, fmt->vars[i].name, keylen)) {
      LOG_WARNING("The abbreviation '%s' is deprecated. "
          "Please use '%s' instead", key, fmt->vars[i].name);
      return (&fmt->vars[i]);
    }
  }

  /* Okay, unrecognized option */
  return (NULL);
}

void confparse_free_lines(configline *front)
{
  configline *tmp;

  while (front) {
    tmp = front;
    front = tmp->next;

    FREE(tmp->key);
    FREE(tmp->value);
    FREE(tmp);
  }
}

/** Release storage held by <b>options</b>. */
void confparse_free(const configformat *fmt, void *options)
{
  int i;

  if (!options)
    return;

  assert(fmt);

  for (i=0; fmt->vars[i].name; ++i)
    clear(fmt, options, &(fmt->vars[i]));

  if (fmt->extra) {
    configline **linep = STRUCT_VAR_P(options, fmt->extra->var_offset);
    confparse_free_lines(*linep);
    *linep = NULL;
  }

  FREE(options);
}

int confparse_get_lines(const char *string, configline **result, int extended)
{
  configline *list = NULL, **next;
  char *k, *v;
  const char *parse_err;

  next = &list;

  do {
    k = v = NULL;
    string = confparse_line_from_str_verbose(string, &k, &v, &parse_err);
    if (!string) {
      LOG_WARNING("Error while parsing configuration: %s",
          parse_err?parse_err:"<unknown>");
      confparse_free_lines(list);
      FREE(k);
      FREE(v);
      return (-1);
    }

    if (k && v) {
      unsigned command = CONFIG_LINE_NORMAL;
      if (extended) {
        if (k[0] == '+') {
          char *k_new = box_strdup(k+1);
          if (!k_new)
            return (-1);
          FREE(k);
          k = k_new;
          command = CONFIG_LINE_APPEND;
        } else if (k[0] == '/') {
          char *k_new = box_strdup(k + 1);
          if (!k_new)
            return (-1);
          FREE(k);
          k = k_new;
          FREE(v);
          v = box_strdup("");
          if (!v)
            return (-1);
          command = CONFIG_LINE_CLEAR;
        }
      }
      /* This list can get long, so we keep a pointer to the end of it
       * rather than using line_append over and over and getting
       * n^2 performance. */
      *next = CALLOC(1, configline);
      (*next)->key = k;
      (*next)->value = v;
      (*next)->next = NULL;
      (*next)->command = command;
      next = &((*next)->next);
    } else {
      FREE(k);
      FREE(v);
    }
  } while (*string);

  *result = list;

  return (0);
}

const char * confparse_line_from_str_verbose(const char *line,
    char **key_out, char **value_out, const char **err_out)
{
  const char *key, *val, *cp;
  int continuation = 0;

  assert(key_out);
  assert(value_out);

  *key_out = *value_out = NULL;
  key = val = NULL;

  /* Skip until the first keyword. */
  while (1) {
    while (isspace(*line))
      ++line;
    if (*line == '#') {
      while (*line && *line != '\n')
        ++line;
    } else {
      break;
    }
  }

  if (!*line) { /* End of string? */
    *key_out = *value_out = NULL;
    return line;
  }

  /* Skip until the next space or \ followed by newline. */
  key = line;

  while (*line && !isspace(*line) && *line != '#' &&
         ! (line[0] == '\\' && line[1] == '\n'))
    ++line;

  *key_out = box_strndup(key, (unsigned long)(line-key));

  if (!*key_out)
    return (NULL);

  /* Skip until the value. */
  while (*line == ' ' || *line == '\t')
    ++line;

  val = line;

  /* Find the end of the line. */
  if (*line == '\"') { // XXX No continuation handling is done here
    if (!(line = unescape_string(line, value_out, NULL))) {
      if (err_out)
        *err_out = "Invalid escape sequence in quoted string";
      return (NULL);
    }

    while (*line == ' ' || *line == '\t')
      ++line;
    if (*line && *line != '#' && *line != '\n') {
      if (err_out)
        *err_out = "Excess data after quoted string";
      return (NULL);
    }
  } else {
    /* Look for the end of the line. */
    while (*line && *line != '\n' && (*line != '#' || continuation)) {
      if (*line == '\\' && line[1] == '\n') {
        continuation = 1;
        line += 2;
      } else if (*line == '#') {
        do {
          ++line;
        } while (*line && *line != '\n');
        if (*line == '\n')
          ++line;
      } else {
        ++line;
      }
    }

    if (*line == '\n') {
      cp = line++;
    } else {
      cp = line;
    }
    /* Now back cp up to be the last nonspace character */
    while (cp > val && isspace(*(cp - 1)))
      --cp;

    assert(cp >= val);

    /* Now copy out and decode the value. */
    *value_out = box_strndup(val, (unsigned long)(cp-val));

    if (!*value_out)
      return (NULL);

    if (continuation) {
      char *v_out, *v_in;
      v_out = v_in = *value_out;
      while (*v_in) {
        if (*v_in == '#') {
          do {
            ++v_in;
          } while (*v_in && *v_in != '\n');
          if (*v_in == '\n')
            ++v_in;
        } else if (v_in[0] == '\\' && v_in[1] == '\n') {
          v_in += 2;
        } else {
          *v_out++ = *v_in++;
        }
      }
      *v_out = '\0';
    }
  }

  if (*line == '#') {
    do {
      ++line;
    } while (*line && *line != '\n');
  }

  while (isspace(*line)) ++line;

  return (line);
}

/** If <b>key</b> is a configuration option, return the corresponding const
 * configvar.  Otherwise, if <b>key</b> is a non-standard abbreviation,
 * warn, and return the corresponding const configvar.  Otherwise return
 * NULL.
 */
const configvar * confparse_find_option(const configformat *fmt, const char *key)
{
  return find_option_mutable(fmt, key);
}

const char * confparse_expand_abbrev(const configformat *fmt, const char *option,
    int command_line, int warn_obsolete)
{
  int i;

  if (! fmt->abbrevs)
    return (option);

  for (i=0; fmt->abbrevs[i].abbreviated; ++i) {
    /* Abbreviations are case insensitive. */
    if (!strcasecmp(option,fmt->abbrevs[i].abbreviated) &&
        (command_line || !fmt->abbrevs[i].commandline_only)) {
      if (warn_obsolete && fmt->abbrevs[i].warn) {
        LOG_WARNING("The configuration option '%s' is deprecated; "
            "use '%s' instead.", fmt->abbrevs[i].abbreviated,
            fmt->abbrevs[i].full);
      }
      /* Keep going through the list in case we want to rewrite it more.
       * (We could imagine recursing here, but I don't want to get the
       * user into an infinite loop if we craft our list wrong.) */
      option = fmt->abbrevs[i].full;
    }
  }

  return (option);
}

/** Set all vars in the configuration object <b>options</b> to their default
 * values. */
void confparse_init(const configformat *fmt, void *options)
{
  int i;
  const configvar *var;

  assert(fmt && options);
  assert((fmt)->magic == *(uint32_t*)STRUCT_VAR_P(options,fmt->magic_offset));

  for (i=0; fmt->vars[i].name; ++i) {
    var = &fmt->vars[i];
    if (!var->initvalue)
      continue; /* defaults to NULL or 0 */
    reset(fmt, options, var, 1);
  }
}

int confparse_assign(const configformat *fmt, void *options, configline *list,
    int use_defaults, int clear_first)
{
  configline *p;
  bitarray_t *options_seen;
  const int n_options = config_count_options(fmt);

  assert(fmt && options);
  assert((fmt)->magic == *(uint32_t*)STRUCT_VAR_P(options,fmt->magic_offset));

  /* pass 1: normalize keys */
  for (p = list; p; p = p->next) {
    const char *full = confparse_expand_abbrev(fmt, p->key, 0, 1);
    if (strcmp(full, p->key)) {
      FREE(p->key);
      p->key = box_strdup(full);
      if (!p->key)
        return (-1);
    }
  }

  /* pass 2: if we're reading from a resetting source, clear all
   * mentioned config options, and maybe set to their defaults. */
  if (clear_first) {
    for (p = list; p; p = p->next)
      reset_line(fmt, options, p->key, use_defaults);
  }

  options_seen = bitarray_init_zero((unsigned int)n_options);

  /* pass 3: assign. */
  while (list) {
    int r;
    if ((r=assign_line(fmt, options, list, use_defaults, clear_first,
        options_seen))) {
      bitarray_free(options_seen);
      return r;
    }
    list = list->next;
  }
  bitarray_free(options_seen);

  /** Now we're done assigning a group of options to the configuration.
   * Subsequent group assignments should _replace_ linelists, not extend
   * them. */
  mark_lists_fragile(fmt, options);

  return (0);
}
