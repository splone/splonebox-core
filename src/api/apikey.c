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

#include <stdlib.h>
#include <stddef.h>

#include "api/sb-api.h"
#include "sb-common.h"


int api_get_key(string key)
{
  size_t i = 0;
  char tmp;
  FILE *f;

  if ((f = fopen("/dev/urandom", "r")) == NULL) {
    LOG_WARNING("Failed to open /dev/urandom!");
    return (-1);
  }

  while (i < key.length -1) {
    if (fread(&tmp, 1, 1, f) != 1) {
      LOG_WARNING("Failed to read from /dev/urandom!");
      return (-1);
    }

    if (!(tmp >= 0x30 && tmp <= 0x39) &&  /* is no numeric */
        !(tmp >= 0x41 && tmp <= 0x5a) &&  /* is no capital letter */
        !(tmp >= 0x61 && tmp <= 0x7a)  /* is no small letters */)
      continue;

    *(key.str + i) = tmp;
    i++;
  }

  *(key.str+i) = '\0';

  if (fclose(f)) {
    LOG_WARNING("Failed to close /dev/urandom!");
    return (-1);
  }

  return (0);
}
