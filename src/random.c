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
 */

#include <stdint.h>
#include <sodium.h>
#include "sb-common.h"

int64_t randommod(long long n)
{
  int64_t result = 0;
  int64_t j;
  unsigned char r[32];
  if (n <= 1)
    return 0;

  randombytes(r,32);

  for (j = 0; j < 32; ++j)
    result = (int64_t)((uint64_t)(result * 256) + (uint64_t)r[j]) % n;

  return result;
}
