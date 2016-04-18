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

#include <ctype.h>
#include "sb-common.h"
#include "parse.h"

/** Yield true iff <b>y</b> is a leap-year. */
#define IS_LEAPYEAR(y) (!(y % 4) && ((y % 100) || !(y % 400)))
/** Helper: Return the number of leap-days between Jan 1, y1 and Jan 1, y2. */
STATIC int n_leapdays(int y1, int y2)
{
  --y1;
  --y2;
  return (y2/4 - y1/4) - (y2/100 - y1/100) + (y2/400 - y1/400);
}

/* Helper: common code to check whether the result of a strtol or strtoul or
 * strtoll is correct. */
#define CHECK_STRTOX_RESULT()                           \
  /* Did an overflow occur? */                          \
  if (errno == ERANGE)                                  \
    goto err;                                           \
  /* Was at least one character converted? */           \
  if (endptr == s)                                      \
    goto err;                                           \
  /* Were there unexpected unconverted characters? */   \
  if (!next && *endptr)                                 \
    goto err;                                           \
  /* Is r within limits? */                             \
  if (r < min || r > max)                               \
    goto err;                                           \
  if (ok) *ok = 1;                                      \
  if (next) *next = endptr;                             \
  return r;                                             \
 err:                                                   \
  if (ok) *ok = 0;                                      \
  if (next) *next = endptr;                             \
  return (0)


/** Number of days per month in non-leap year; used by tor_timegm and
 * parse_rfc1123_time. */
static const int days_per_month[] =
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/** Table to map the names of memory units to the number of bytes they
 * contain. */
static struct unit_table_t memory_units[] = {
  { "",          1 },
  { "b",         1<< 0 },
  { "byte",      1<< 0 },
  { "bytes",     1<< 0 },
  { "kb",        1<<10 },
  { "kbyte",     1<<10 },
  { "kbytes",    1<<10 },
  { "kilobyte",  1<<10 },
  { "kilobytes", 1<<10 },
  { "kilobits",  1<<7  },
  { "kilobit",   1<<7  },
  { "kbits",     1<<7  },
  { "kbit",      1<<7  },
  { "m",         1<<20 },
  { "mb",        1<<20 },
  { "mbyte",     1<<20 },
  { "mbytes",    1<<20 },
  { "megabyte",  1<<20 },
  { "megabytes", 1<<20 },
  { "megabits",  1<<17 },
  { "megabit",   1<<17 },
  { "mbits",     1<<17 },
  { "mbit",      1<<17 },
  { "gb",        1<<30 },
  { "gbyte",     1<<30 },
  { "gbytes",    1<<30 },
  { "gigabyte",  1<<30 },
  { "gigabytes", 1<<30 },
  { "gigabits",  1<<27 },
  { "gigabit",   1<<27 },
  { "gbits",     1<<27 },
  { "gbit",      1<<27 },
  { NULL, 0 },
};

/** Table to map the names of time units to the number of seconds they
 * contain. */
static struct unit_table_t time_units[] = {
  { "",         1 },
  { "second",   1 },
  { "seconds",  1 },
  { "minute",   60 },
  { "minutes",  60 },
  { "hour",     60*60 },
  { "hours",    60*60 },
  { "day",      24*60*60 },
  { "days",     24*60*60 },
  { "week",     7*24*60*60 },
  { "weeks",    7*24*60*60 },
  { "month",    2629728, }, /* about 30.437 days */
  { "months",   2629728, },
  { NULL, 0 },
};

/** Table to map the names of time units to the number of milliseconds
 * they contain. */
static struct unit_table_t time_msec_units[] = {
  { "",         1 },
  { "msec",     1 },
  { "millisecond", 1 },
  { "milliseconds", 1 },
  { "second",   1000 },
  { "seconds",  1000 },
  { "minute",   60*1000 },
  { "minutes",  60*1000 },
  { "hour",     60*60*1000 },
  { "hours",    60*60*1000 },
  { "day",      24*60*60*1000 },
  { "days",     24*60*60*1000 },
  { "week",     7*24*60*60*1000 },
  { "weeks",    7*24*60*60*1000 },
  { NULL, 0 },
};

/** Extract a long from the start of <b>s</b>, in the given numeric
 * <b>base</b>.  If <b>base</b> is 0, <b>s</b> is parsed as a decimal,
 * octal, or hex number in the syntax of a C integer literal.  If
 * there is unconverted data and <b>next</b> is provided, set
 * *<b>next</b> to the first unconverted character.  An error has
 * occurred if no characters are converted; or if there are
 * unconverted characters and <b>next</b> is NULL; or if the parsed
 * value is not between <b>min</b> and <b>max</b>.  When no error
 * occurs, return the parsed value and set *<b>ok</b> (if provided) to
 * 1.  When an error occurs, return 0 and set *<b>ok</b> (if provided)
 * to 0.
 */
long parse_long(const char *s, int base, long min, long max, int *ok,
    char **next)
{
  char *endptr;
  long r;

  if (base < 0) {
    if (ok)
      *ok = 0;
    return (0);
  }

  errno = 0;
  r = strtol(s, &endptr, base);
  CHECK_STRTOX_RESULT();
}

/** As tor_parse_long(), but return an unsigned long. */
unsigned long parse_ulong(const char *s, int base, unsigned long min,
    unsigned long max, int *ok, char **next)
{
  char *endptr;
  unsigned long r;

  if (base < 0) {
    if (ok)
      *ok = 0;
    return (0);
  }

  errno = 0;
  r = strtoul(s, &endptr, base);
  CHECK_STRTOX_RESULT();
}

/** As tor_parse_long(), but return a double. */
double parse_double(const char *s, double min, double max, int *ok,
    char **next)
{
  char *endptr;
  double r;

  errno = 0;
  r = strtod(s, &endptr);
  CHECK_STRTOX_RESULT();
}

/** As tor_parse_long, but return a uint64_t.  Only base 10 is guaranteed to
 * work for now. */
uint64_t parse_uint64(const char *s, int base, uint64_t min, uint64_t max,
    int *ok, char **next)
{
  char *endptr;
  uint64_t r;

  if (base < 0) {
    if (ok)
      *ok = 0;
    return (0);
  }

  errno = 0;
#ifdef HAVE_STRTOULL
  r = (uint64_t)strtoull(s, &endptr, base);
#elif defined(ARCH_64)
  r = (uint64_t)strtoul(s, &endptr, base);
#else
#error "I don't know how to parse 64-bit numbers."
#endif

  CHECK_STRTOX_RESULT();
}

/** Parse a string <b>val</b> containing a number, zero or more
 * spaces, and an optional unit string.  If the unit appears in the
 * table <b>u</b>, then multiply the number by the unit multiplier.
 * On success, set *<b>ok</b> to 1 and return this product.
 * Otherwise, set *<b>ok</b> to 0.
 */
uint64_t parse_units(const char *val, struct unit_table_t *u, int *ok)
{
  uint64_t v = 0;
  double d = 0;
  int use_float = 0;
  char *cp;

  sbassert(ok);

  v = parse_uint64(val, 10, 0, UINT64_MAX, ok, &cp);

  if (!*ok || (cp && *cp == '.')) {
    d = parse_double(val, 0, UINT64_MAX, ok, &cp);

    if (!*ok)
      goto done;
    use_float = 1;
  }

  if (!cp) {
    *ok = 1;
    v = use_float ? (uint64_t)(d) :  v;

    goto done;
  }

  cp = (char*) eat_whitespace(cp);

  for ( ; u->unit; ++u) {
    if (!strcasecmp(u->unit, cp)) {
      if (use_float)
        v = (uint64_t)(u->multiplier * d);
      else
        v *= u->multiplier;
      *ok = 1;

      goto done;
    }
  }

  LOG_WARNING("Unknown unit '%s'.", cp);
  *ok = 0;

done:

  if (*ok)
    return (v);
  else
    return (0);
}

/** Parse a string in the format "number unit", where unit is a unit of
 * information (byte, KB, M, etc).  On success, set *<b>ok</b> to true
 * and return the number of bytes specified.  Otherwise, set
 * *<b>ok</b> to false and return (0). */
uint64_t parse_memunit(const char *s, int *ok)
{
  uint64_t u = parse_units(s, memory_units, ok);
  return (u);
}

/** Parse a string in the format "number unit", where unit is a unit of
 * time in milliseconds.  On success, set *<b>ok</b> to true and return
 * the number of milliseconds in the provided interval.  Otherwise, set
 * *<b>ok</b> to 0 and return (-1). */
int parse_msec_interval(const char *s, int *ok)
{
  uint64_t r;
  r = parse_units(s, time_msec_units, ok);

  if (!ok)
    return (-1);

  if (r > INT_MAX) {
    LOG_WARNING("Msec interval '%s' is too long", s);
    *ok = 0;

    return (-1);
  }

  return (int)r;
}

/** Parse a string in the format "number unit", where unit is a unit of time.
 * On success, set *<b>ok</b> to true and return the number of seconds in
 * the provided interval.  Otherwise, set *<b>ok</b> to 0 and return (-1).
 */
int parse_interval(const char *s, int *ok)
{
  uint64_t r;
  r = parse_units(s, time_units, ok);

  if (!ok)
    return (-1);

  if (r > INT_MAX) {
    LOG_WARNING("Interval '%s' is too long", s);
    *ok = 0;

    return (-1);
  }

  return (int)r;
}

/** Given an ISO-formatted UTC time value (after the epoch) in <b>cp</b>,
 * parse it and store its value in *<b>t</b>.  Return 0 on success, -1 on
 * failure.  Ignore extraneous stuff in <b>cp</b> after the end of the time
 * string, unless <b>strict</b> is set. */
int parse_iso_time(const char *cp, time_t *t)
{
  struct tm st_tm;
  unsigned int year=0, month=0, day=0, hour=0, minute=0, second=0;
  int n_fields;
  char extra_char;

  n_fields = box_sscanf(cp, "%u-%2u-%2u %2u:%2u:%2u%c", &year, &month, &day,
      &hour, &minute, &second, &extra_char);

  if (n_fields != 6) {
    LOG_WARNING("ISO time %s was unparseable", cp);
    return (-1);
  }

  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour > 23 || minute > 59 || second > 60 || year >= INT32_MAX) {
    LOG_WARNING("ISO time %s was nonsensical", cp);
    return (-1);
  }

  st_tm.tm_year = (int)year - 1900;
  st_tm.tm_mon = (int)month - 1;
  st_tm.tm_mday = (int)day;
  st_tm.tm_hour = (int)hour;
  st_tm.tm_min = (int)minute;
  st_tm.tm_sec = (int)second;

  if (st_tm.tm_year < 70) {
    LOG_WARNING("Got invalid ISO time %s. (Before 1970)", cp);
    return (-1);
  }

  return parse_timegm(&st_tm, t);
}

/** Compute a time_t given a struct tm.  The result is given in UTC, and
 * does not account for leap seconds.  Return 0 on success, -1 on failure.
 */
int parse_timegm(const struct tm *tm, time_t *time_out)
{
  /* This is a pretty ironclad timegm implementation, snarfed from Python2.2.
   * It's way more brute-force than fiddling with tzset().
   */
  time_t year, days, hours, minutes, seconds;
  int i, invalid_year, dpm;

  /* avoid int overflow on addition */
  if (tm->tm_year < INT32_MAX - 1900) {
    year = tm->tm_year + 1900;
  } else {
    /* clamp year */
    year = INT32_MAX;
  }

  invalid_year = (year < 1970 || tm->tm_year >= INT32_MAX - 1900);

  if (tm->tm_mon >= 0 && tm->tm_mon <= 11) {
    dpm = days_per_month[tm->tm_mon];
    if (tm->tm_mon == 1 && !invalid_year && IS_LEAPYEAR(tm->tm_year)) {
      dpm = 29;
    }
  } else {
    /* invalid month - default to 0 days per month */
    dpm = 0;
  }

  if (invalid_year || tm->tm_mon < 0 || tm->tm_mon > 11 ||
      tm->tm_mday < 1 || tm->tm_mday > dpm ||
      tm->tm_hour < 0 || tm->tm_hour > 23 ||
      tm->tm_min < 0 || tm->tm_min > 59 ||
      tm->tm_sec < 0 || tm->tm_sec > 60) {
    LOG_WARNING("Out-of-range argument to tor_timegm");
    return (-1);
  }

  days = 365 * (year - 1970) + n_leapdays(1970, (int)year);

  for (i = 0; i < tm->tm_mon; ++i)
    days += days_per_month[i];

  if (tm->tm_mon > 1 && IS_LEAPYEAR(year))
    ++days;

  days += tm->tm_mday - 1;
  hours = days*24 + tm->tm_hour;

  minutes = hours*60 + tm->tm_min;
  seconds = minutes*60 + tm->tm_sec;
  *time_out = seconds;

  return (0);
}
