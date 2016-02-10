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
#include "sb-common.h"
#include "helper-unix.h"


void functional_db_function_verify(UNUSED(void **state))
{
  string apikey = cstring_copy_string(
      "bkAhXpRUwuwdeTx0tc24xXDPl6RdIH1uVgQUGRhAZTTjdYiYqkmTmVXgZmRSWuKi");
  string name = cstring_copy_string("name of function");
  string desc = cstring_copy_string(
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
  struct message_params_object params, *args;

  params.size = 4;
  params.obj =  CALLOC(params.size, struct message_object);

  params.obj[0].type = OBJECT_TYPE_STR;
  params.obj[0].data.string = name;

  params.obj[1].type = OBJECT_TYPE_STR;
  params.obj[1].data.string = desc;

  params.obj[2].type = OBJECT_TYPE_ARRAY;
  params.obj[2].data.params.size = 1;
  params.obj[2].data.params.obj = CALLOC(params.obj[2].data.params.size,
                                        struct message_object);
  params.obj[2].data.params.obj[0].type = OBJECT_TYPE_INT;
  params.obj[2].data.params.obj[0].data.uinteger = 6;

  args = &params.obj[2].data.params;

  connect_and_create(apikey);

  /* registering function should work */
  assert_int_equal(0, db_function_add(apikey, &params));

  /* verifying a valid function call should work properly */
  assert_int_equal(0, db_function_verify(apikey, name, args));

  /* verifying a non-existing function  */
  string invalid_name = cstring_copy_string("foobar");
  string invalid_apikey = cstring_copy_string("bkAhXpRU");
  assert_int_not_equal(0, db_function_verify(apikey, invalid_name, args));
  assert_int_not_equal(0, db_function_verify(invalid_apikey, name, args));

  /* verifying an existing function with wrong arg count */
  args->size = 2;
  assert_int_not_equal(0, db_function_verify(apikey, name, args));

  args->size = SIZE_MAX;
  assert_int_not_equal(0, db_function_verify(apikey, name, args));

  /* verifying an existing function with wrong arguments' type */
  args->size = 1;
  args->obj[0].type = OBJECT_TYPE_UINT;
  args->obj[0].data.integer = -1;
  assert_int_not_equal(0, db_function_verify(apikey, name, args));

  free_params(params);
  free_string(apikey);
  free_string(invalid_name);
  free_string(invalid_apikey);
  db_close();
}
