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
#include "sb-common.h"
#include "helper-unix.h"


void functional_db_function_add(UNUSED(void **state))
{
  string apikey = cstring_copy_string(
      "bkAhXpRUwuwdeTx0tc24xXDPl6RdIH1uVgQUGRhAZTTjdYiYqkmTmVXgZmRSWuKi");
  string name = cstring_copy_string(
      "name of function");
  string desc = cstring_copy_string(
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
  struct message_params_object params;

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

  connect_and_create(apikey);

  /* valid function params should work */
  assert_int_equal(0, db_function_add(apikey, &params));

  /* empty function params should not work */
  params.size = 0;
  assert_int_not_equal(0, db_function_add(apikey, &params));

  /* function without arguments should work */
  params.size = 4;
  params.obj[2].data.params.size = 0;
  assert_int_equal(0, db_function_add(apikey, &params));

  /* function without name should not work */
  params.size = 4;
  params.obj[2].data.params.size = 1;
  params.obj[0].data.string = cstring_copy_string("");
  assert_int_not_equal(0, db_function_add(apikey, &params));
  free_string(params.obj[0].data.string);

  /* function without description should work */
  params.obj[0].data.string = name;
  params.obj[1].data.string = cstring_copy_string("");
  assert_int_equal(0, db_function_add(apikey, &params));

  db_close();

  free_params(params);
  free_string(apikey);
  free_string(desc);
}
