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

#include <stdbool.h>

#include "helper-all.h"
#include "rpc/sb-rpc.h"
#include "sb-common.h"
#include "helper-unix.h"
#include "rpc/db/sb-db.h"
#include "api/sb-api.h"

void functional_db_apikey_verify(UNUSED(void **state))
{
  string apikey;
  string invalid;
  string empty;
  size_t n = 64;

  apikey.str = MALLOC_ARRAY(n, char);
  if (!apikey.str)
    LOG_ERROR("Failed to allocate mem for api key string.");
  apikey.length = n;

  assert_int_equal(0, api_get_key(apikey));
  connect_to_db();

  assert_int_equal(0, db_apikey_add(apikey));

  /* verify correct key should succeed */
  assert_int_equal(0, db_apikey_verify(apikey));

  /* verify incorrect keys should fail */
  invalid = cstring_copy_string("foobar");
  assert_int_not_equal(0, db_apikey_verify(invalid));

  empty = cstring_copy_string("");
  assert_int_not_equal(0, db_apikey_verify(empty));

  free_string(apikey);
  free_string(invalid);
  free_string(empty);

  db_close();
}
