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

#include "helper-unix.h"
#include "helper-all.h"
#include "sb-common.h"
#include "api/sb-api.h"
#include "rpc/db/sb-db.h"

void functional_db_apikey_add(UNUSED(void **state))
{
  string apikey;
  size_t n = 64;

  apikey.str = MALLOC_ARRAY(n, char);
  if (!apikey.str)
    LOG_ERROR("Failed to allocate mem for api key string.");
  apikey.length = n;

  assert_int_equal(0, api_get_key(apikey));
  connect_to_db();

  assert_int_equal(0, db_apikey_add(apikey));

  db_close();
  free_string(apikey);
}
