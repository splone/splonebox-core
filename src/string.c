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

#include <stddef.h>
#include <string.h>
#ifdef __linux__
#include <bsd/string.h>
#endif
#include "sb-common.h"


string cstring_to_string(char *str)
{
  if (!str)
    return (string) {.str = NULL, .length = 0};

  return (string) {.str = str, .length = strlen(str)};
}

string cstring_copy_string(const char *str)
{
  size_t length;
  char *ret;

  if (!str)
    return (string) STRING_INIT;

  length = strlen(str);
  ret = MALLOC_ARRAY(length + 1, char);

  strlcpy(ret, str, length + 1);

  return (string) {.str = ret, .length = length};
}

void free_string(string str)
{
  if (!str.str) {
    return;
  }

  FREE(str.str);
}
