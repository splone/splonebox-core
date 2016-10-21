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
#include <stdlib.h>

#include "helper-all.h"
#include "rpc/sb-rpc.h"
#include "api/sb-api.h"
#include "sb-common.h"
#include "helper-unix.h"


void functional_db_function_add(UNUSED(void **state))
{
  char pluginkey[PLUGINKEY_STRING_SIZE] = "012345789ABCDEFH";
  string name = cstring_copy_string(
      "name of function");
  string desc = cstring_copy_string(
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
  array params;

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

  connect_and_create(pluginkey);

  /* valid function params should work */
  assert_int_equal(0, db_function_add(pluginkey, &params));

  /* empty function params should not work */
  params.size = 0;
  assert_int_not_equal(0, db_function_add(pluginkey, &params));

  /* function without arguments should work */
  params.size = 4;
  params.items[2].data.array.size = 0;
  assert_int_equal(0, db_function_add(pluginkey, &params));

  /* function without name should not work */
  params.size = 4;
  params.items[2].data.array.size = 1;
  params.items[0].data.string = cstring_copy_string("");
  assert_int_not_equal(0, db_function_add(pluginkey, &params));
  free_string(params.items[0].data.string);

  /* function without description should work */
  params.items[0].data.string = name;
  params.items[1].data.string = cstring_copy_string("");
  assert_int_equal(0, db_function_add(pluginkey, &params));

  db_close();

  api_free_array(params);
  api_free_string(desc);
}
