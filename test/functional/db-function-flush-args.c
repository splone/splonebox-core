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
#include "rpc/db/sb-db.h"
#include "sb-common.h"
#include "helper-unix.h"


static ssize_t db_function_get_argc(string api_key, string name)
{
  redisReply *reply;
  ssize_t result;

  if (!rc) {
    LOG_WARNING("No redis connection available!");
    return -1;
  }

  reply = redisCommand(rc, "LLEN %s:func:%s:args", api_key.str,
                       name.str);

  if (reply->type != REDIS_REPLY_INTEGER) {
    LOG_WARNING("Redis failed to get arguments list length: %s", reply->str);
    freeReplyObject(reply);

    return -1;
  }

  result = reply->integer;
  freeReplyObject(reply);

  return result;
}

void functional_db_function_flush_args(UNUSED(void **state))
{
  string apikey = cstring_copy_string(
      "bkAhXpRUwuwdeTx0tc24xXDPl6RdIH1uVgQUGRhAZTTjdYiYqkmTmVXgZmRSWuKi");
  string name = cstring_copy_string("name of function");
  string desc = cstring_copy_string(
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
  struct message_params_object params, *args;
  ssize_t argc = 0;

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
  args = &params.obj[2].data.params;

  /* register function */
  assert_int_equal(0, db_function_add(apikey, &params));

  /* verify that registration succeeded */
  argc = db_function_get_argc(apikey, name);
  assert_true(argc >= 0);
  assert_true((size_t)argc == args->size);

  /*
   * regression test:
   * if flushings works, re-registering does not change the argc
   */
  assert_int_equal(0, db_function_add(apikey, &params));
  argc = db_function_get_argc(apikey, name);
  assert_true(argc >= 0);
  assert_true((size_t)argc == args->size);


  /* now test with two arguments */
  free_params(params.obj[2].data.params);
  params.obj[2].data.params.size = 2;
  params.obj[2].data.params.obj = CALLOC(params.obj[2].data.params.size,
                                        struct message_object);
  params.obj[2].data.params.obj[0].type = OBJECT_TYPE_INT;
  params.obj[2].data.params.obj[0].data.uinteger = 6;
  params.obj[2].data.params.obj[0].type = OBJECT_TYPE_INT;
  params.obj[2].data.params.obj[0].data.uinteger = 12;

  assert_int_equal(0, db_function_add(apikey, &params));
  argc = db_function_get_argc(apikey, name);
  assert_true(argc >= 0);
  assert_true((size_t)argc == args->size);

  assert_int_equal(0, db_function_add(apikey, &params));
  argc = db_function_get_argc(apikey, name);
  assert_true(argc >= 0);
  assert_true((size_t)argc == args->size);

  db_close();

  free_params(params);
  free_string(apikey);
}
