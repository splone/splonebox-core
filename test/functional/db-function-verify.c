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

#include <hiredis/hiredis.h>
#include <stdint.h>
#include <stdlib.h>

#include "helper-all.h"
#include "rpc/sb-rpc.h"
#include "api/sb-api.h"
#include "sb-common.h"
#include "helper-unix.h"


void functional_db_function_verify(UNUSED(void **state))
{
  char pluginkey[PLUGINKEY_STRING_SIZE] = "012345789ABCDEFH";
  string name = cstring_copy_string("name of function");
  string desc = cstring_copy_string(
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
  array params, *args;

  params.size = 4;
  params.items =  CALLOC(params.size, object);

  params.items[0].type = OBJECT_TYPE_STR;
  params.items[0].data.string = name;

  params.items[1].type = OBJECT_TYPE_STR;
  params.items[1].data.string = desc;

  params.items[2].type = OBJECT_TYPE_ARRAY;
  params.items[2].data.array.size = 1;
  params.items[2].data.array.items = CALLOC(params.items[2].data.array.size,
      object);
  params.items[2].data.array.items[0].type = OBJECT_TYPE_INT;
  params.items[2].data.array.items[0].data.uinteger = 6;

  args = &params.items[2].data.array;

  connect_and_create(pluginkey);

  /* registering function should work */
  assert_int_equal(0, db_function_add(pluginkey, &params));

  /* verifying a valid function call should work properly */
  assert_int_equal(0, db_function_verify(pluginkey, name, args));

  /* verifying a non-existing function  */
  string invalid_name = cstring_copy_string("foobar");
  char invalid_pluginkey[PLUGINKEY_STRING_SIZE] = "FFFFFFFFFFFFFFFF";
  assert_int_not_equal(0, db_function_verify(pluginkey, invalid_name, args));
  assert_int_not_equal(0, db_function_verify(invalid_pluginkey, name, args));

  /* verifying an existing function with wrong arg count */
  args->size = 2;
  assert_int_not_equal(0, db_function_verify(pluginkey, name, args));

  args->size = SIZE_MAX;
  assert_int_not_equal(0, db_function_verify(pluginkey, name, args));

  /* verifying an existing function with wrong arguments' type */
  args->size = 1;
  args->items[0].type = OBJECT_TYPE_UINT;
  args->items[0].data.integer = -1;
  assert_int_not_equal(0, db_function_verify(pluginkey, name, args));

  api_free_array(params);
  api_free_string(invalid_name);
  db_close();
}
