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

#include "sb-common.h"
#include "util.h"

#define MAX_SCANF_WIDTH 9999

/** Helper: given an ASCII-encoded decimal digit, return its numeric value.
 * NOTE: requires that its input be in-bounds. */
STATIC int digit_to_num(char d)
{
  int num = ((int)d) - (int)'0';
  sbassert(num <= 9 && num >= 0);

  return (num);
}

/** Helper: Read an unsigned int from *<b>bufp</b> of up to <b>width</b>
 * characters.  (Handle arbitrary width if <b>width</b> is less than 0.)  On
 * success, store the result in <b>out</b>, advance bufp to the next
 * character, and return (0).  On failure, return (-1). */
STATIC int scan_unsigned(const char **bufp, unsigned long *out, int width,
    int base)
{
  unsigned long result = 0;
  int scanned_so_far = 0;
  const int hex = base==16;

  sbassert(base == 10 || base == 16);

  if (!bufp || !*bufp || !out)
    return (-1);
  if (width<0)
    width=MAX_SCANF_WIDTH;

  while (**bufp && (hex?ISXDIGIT(**bufp):ISDIGIT(**bufp))
         && scanned_so_far < width) {
    int digit = hex ? hex_decode_digit(*(*bufp)++) : digit_to_num(*(*bufp)++);
    // Check for overflow beforehand, without actually causing any overflow
    // This preserves functionality on compilers that don't wrap overflow
    // (i.e. that trap or optimise away overflow)
    // result * base + digit > ULONG_MAX
    // result * base > ULONG_MAX - digit
    if (result > (ULONG_MAX - (unsigned long)digit)/ (unsigned long)base)
      return (-1); /* Processing this digit would overflow */
    result = (result * (unsigned long)base) + (unsigned long)digit;
    ++scanned_so_far;
  }

  if (!scanned_so_far) /* No actual digits scanned */
    return (-1);

  *out = result;

  return (0);
}

/** Helper: Read an signed int from *<b>bufp</b> of up to <b>width</b>
 * characters.  (Handle arbitrary width if <b>width</b> is less than 0.)  On
 * success, store the result in <b>out</b>, advance bufp to the next
 * character, and return (0).  On failure, return (-1). */
STATIC int scan_signed(const char **bufp, long *out, int width)
{
  int neg = 0;
  unsigned long result = 0;

  if (!bufp || !*bufp || !out)
    return (-1);
  if (width<0)
    width=MAX_SCANF_WIDTH;

  if (**bufp == '-') {
    neg = 1;
    ++*bufp;
    --width;
  }

  if (scan_unsigned(bufp, &result, width, 10) < 0)
    return (-1);

  if (neg && result > 0) {
    if (result > ((unsigned long)LONG_MAX) + 1)
      return (-1); /* Underflow */
    // Avoid overflow on the cast to signed long when result is LONG_MIN
    // by subtracting 1 from the unsigned long positive value,
    // then, after it has been cast to signed and negated,
    // subtracting the original 1 (the double-subtraction is intentional).
    // Otherwise, the cast to signed could cause a temporary long
    // to equal LONG_MAX + 1, which is undefined.
    // We avoid underflow on the subtraction by treating -0 as positive.
    *out = (-(long)(result - 1)) - 1;
  } else {
    if (result > LONG_MAX)
      return (-1); /* Overflow */
    *out = (long)result;
  }

  return (0);
}

/** Helper: Read a decimal-formatted double from *<b>bufp</b> of up to
 * <b>width</b> characters.  (Handle arbitrary width if <b>width</b> is less
 * than 0.)  On success, store the result in <b>out</b>, advance bufp to the
 * next character, and return (0).  On failure, return (-1). */
STATIC int scan_double(const char **bufp, double *out, int width)
{
  int neg = 0;
  double result = 0;
  int scanned_so_far = 0;

  if (!bufp || !*bufp || !out)
    return (-1);
  if (width<0)
    width=MAX_SCANF_WIDTH;

  if (**bufp == '-') {
    neg = 1;
    ++*bufp;
  }

  while (**bufp && ISDIGIT(**bufp) && scanned_so_far < width) {
    const int digit = digit_to_num(*(*bufp)++);
    result = result * 10 + digit;
    ++scanned_so_far;
  }
  if (**bufp == '.') {
    double fracval = 0, denominator = 1;
    ++*bufp;
    ++scanned_so_far;
    while (**bufp && ISDIGIT(**bufp) && scanned_so_far < width) {
      const int digit = digit_to_num(*(*bufp)++);
      fracval = fracval * 10 + digit;
      denominator *= 10;
      ++scanned_so_far;
    }
    result += fracval / denominator;
  }

  if (!scanned_so_far) /* No actual digits scanned */
    return (-1);

  *out = neg ? -result : result;

  return (0);
}

/** Helper: copy up to <b>width</b> non-space characters from <b>bufp</b> to
 * <b>out</b>.  Make sure <b>out</b> is nul-terminated. Advance <b>bufp</b>
 * to the next non-space character or the EOS. */
STATIC int scan_string(const char **bufp, char *out, int width)
{
  int scanned_so_far = 0;

  if (!bufp || !out || width < 0)
    return (-1);

  while (**bufp && ! ISSPACE(**bufp) && scanned_so_far < width) {
    *out++ = *(*bufp)++;
    ++scanned_so_far;
  }
  *out = '\0';

  return (0);
}


char * box_strdup(const char *s)
{
  char *dup;

  sbassert(s);

  dup = strdup(s);

  if (dup == NULL) {
    LOG_ERROR("Out of memory on strdup(). Dying.");
  }

  return (dup);
}

/** Allocate and return a new string containing the first <b>n</b>
 * characters of <b>s</b>.  If <b>s</b> is longer than <b>n</b>
 * characters, only the first <b>n</b> are copied.  The result is
 * always NUL-terminated.  (Like strndup(s,n), but never returns
 * NULL.)
 */
char * box_strndup(const char *s, size_t n)
{
  char *dup;

  sbassert(s);

  dup = MALLOC_ARRAY((n+1), char);

  strncpy(dup, s, n);
  dup[n]='\0';

  return (dup);
}

/** Minimal sscanf replacement: parse <b>buf</b> according to <b>pattern</b>
 * and store the results in the corresponding argument fields.  Differs from
 * sscanf in that:
 * <ul><li>It only handles %u, %lu, %x, %lx, %[NUM]s, %d, %ld, %lf, and %c.
 *     <li>It only handles decimal inputs for %lf. (12.3, not 1.23e1)
 *     <li>It does not handle arbitrarily long widths.
 *     <li>Numbers do not consume any space characters.
 *     <li>It is locale-independent.
 *     <li>%u and %x do not consume any space.
 *     <li>It returns -1 on malformed patterns.</ul>
 *
 * (As with other locale-independent functions, we need this to parse data that
 * is in ASCII without worrying that the C library's locale-handling will make
 * miscellaneous characters look like numbers, spaces, and so on.)
 */
int box_sscanf(const char *buf, const char *pattern, ...)
{
  int r;

  va_list ap;
  va_start(ap, pattern);
  r = box_vsscanf(buf, pattern, ap);
  va_end(ap);

  return (r);
}

/** Locale-independent, minimal, no-surprises scanf variant, accepting only a
 * restricted pattern format.  For more info on what it supports, see
 * box_sscanf() documentation.  */
int box_vsscanf(const char *buf, const char *pattern, va_list ap)
{
  int n_matched = 0;

  while (*pattern) {
    if (*pattern != '%') {
      if (*buf == *pattern) {
        ++buf;
        ++pattern;
        continue;
      } else {
        return n_matched;
      }
    } else {
      int width = -1;
      int longmod = 0;
      ++pattern;
      if (ISDIGIT(*pattern)) {
        width = digit_to_num(*pattern++);
        while (ISDIGIT(*pattern)) {
          width *= 10;
          width += digit_to_num(*pattern++);
          if (width > MAX_SCANF_WIDTH)
            return (-1);
        }
        if (!width) /* No zero-width things. */
          return (-1);
      }
      if (*pattern == 'l') {
        longmod = 1;
        ++pattern;
      }
      if (*pattern == 'u' || *pattern == 'x') {
        unsigned long u;
        const int base = (*pattern == 'u') ? 10 : 16;
        if (!*buf)
          return n_matched;
        if (scan_unsigned(&buf, &u, width, base)<0)
          return n_matched;
        if (longmod) {
          unsigned long *out = va_arg(ap, unsigned long *);
          *out = u;
        } else {
          unsigned *out = va_arg(ap, unsigned *);
          if (u > UINT_MAX)
            return n_matched;
          *out = (unsigned) u;
        }
        ++pattern;
        ++n_matched;
      } else if (*pattern == 'f') {
        double *d = va_arg(ap, double *);
        if (!longmod)
          return (-1); /* float not supported */
        if (!*buf)
          return n_matched;
        if (scan_double(&buf, d, width)<0)
          return n_matched;
        ++pattern;
        ++n_matched;
      } else if (*pattern == 'd') {
        long lng=0;
        if (scan_signed(&buf, &lng, width)<0)
          return n_matched;
        if (longmod) {
          long *out = va_arg(ap, long *);
          *out = lng;
        } else {
          int *out = va_arg(ap, int *);
          if (lng < INT_MIN || lng > INT_MAX)
            return n_matched;
          *out = (int)lng;
        }
        ++pattern;
        ++n_matched;
      } else if (*pattern == 's') {
        char *s = va_arg(ap, char *);
        if (longmod)
          return (-1);
        if (width < 0)
          return (-1);
        if (scan_string(&buf, s, width)<0)
          return n_matched;
        ++pattern;
        ++n_matched;
      } else if (*pattern == 'c') {
        char *ch = va_arg(ap, char *);
        if (longmod)
          return (-1);
        if (width != -1)
          return (-1);
        if (!*buf)
          return n_matched;
        *ch = *buf++;
        ++pattern;
        ++n_matched;
      } else if (*pattern == '%') {
        if (*buf != '%')
          return n_matched;
        if (longmod)
          return (-1);
        ++buf;
        ++pattern;
      } else {
        return (-1); /* Unrecognized pattern component. */
      }
    }
  }

  return (n_matched);
}

/** Return a pointer to the first char of s that is not whitespace and
 * not a comment, or to the terminating NUL if no such character exists.
 */
const char * eat_whitespace(const char *s)
{
  sbassert(s);

  while (1) {
    switch (*s) {
    case '\0':
    default:
      return s;
    case ' ':
    case '\t':
    case '\n':
    case '\r':
      ++s;
      break;
    case '#':
      ++s;
      while (*s && *s != '\n')
        ++s;
    }
  }
}

/** Given a hexadecimal string of <b>srclen</b> bytes in <b>src</b>, decode
 * it and store the result in the <b>destlen</b>-byte buffer at <b>dest</b>.
 * Return the number of bytes decoded on success, -1 on failure. If
 * <b>destlen</b> is greater than INT_MAX or less than half of
 * <b>srclen</b>, -1 is returned. */
int base16_decode(char *dest, size_t destlen, const char *src, size_t srclen)
{
  const char *end;
  char *dest_orig = dest;
  int v1,v2;

  if ((srclen % 2) != 0)
    return -1;
  if (destlen < srclen/2 || destlen > INT_MAX)
    return -1;

  /* Make sure we leave no uninitialized data in the destination buffer. */
  memset(dest, 0, destlen);

  end = src+srclen;
  while (src<end) {
    v1 = hex_decode_digit(*src);
    v2 = hex_decode_digit(*(src+1));
    if (v1<0||v2<0)
      return -1;
    *(uint8_t*)dest = (v1<<4)|v2;
    ++dest;
    src+=2;
  }

  sbassert((dest-dest_orig) <= (ptrdiff_t) destlen);

  return (int) (dest-dest_orig);
}
