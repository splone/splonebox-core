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
 *    Copyright (c) 2013-2016
 *    Frank Denis <j at pureftpd dot org>
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "sb-common.h"

#ifdef HAVE_WEAK_SYMBOLS
__attribute__ ((weak)) void _sb_dummy_symbol_to_prevent_memzero_lto(
    void * const pnt, const size_t len)
{
  (void) pnt;
  (void) len;
}
#endif

void sbmemzero(void * const pnt, const size_t len)
{
#if defined(HAVE_MEMSET_S)
  if (memset_s(pnt, (rsize_t) len, 0, (rsize_t) len) != 0) {
    abort();
  }
#elif defined(HAVE_EXPLICIT_BZERO)
  explicit_bzero(pnt, len);
#elif defined(HAVE_WEAK_SYMBOLS)
  memset(pnt, 0, len);
  _sb_dummy_symbol_to_prevent_memzero_lto(pnt, len);
#else
  volatile unsigned char *volatile pnt_ =
      (volatile unsigned char * volatile) pnt;
  size_t i = (size_t) 0U;

  while (i < len) {
    pnt_[i++] = 0U;
  }
 #endif
}
