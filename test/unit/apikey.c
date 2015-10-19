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

#include "sb-common.h"
#include "api/sb-api.h"
#include "helper-unix.h"

static int validate_apikey(const size_t n)
{
  string apikey;

  apikey.str = MALLOC_ARRAY(n, char);
  if (!apikey.str) {
    LOG_WARNING("Failed to allocate mem for api key string.");
    return (-1);
  }

  apikey.length = n;

  assert_int_equal(0, api_get_key(apikey));
  assert_true(apikey.str[n-1] == '\0');

  LOG("generated key: %s\n", apikey.str);

  free_string(apikey);
  return (0);
}


void unit_api_get_key(UNUSED(void **state))
{
  assert_int_equal(0, validate_apikey(10));
  assert_int_equal(0, validate_apikey(32));
  assert_int_equal(0, validate_apikey(64));
  assert_int_equal(0, validate_apikey(128));
}
