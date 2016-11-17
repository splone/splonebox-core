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

#include <msgpack.h>

#include "sb-common.h"
#include "tweetnacl.h"
#include "rpc/sb-rpc.h"
#include "rpc/connection/connection.h"
#include "api/helpers.h"
#include "api/sb-api.h"
#ifdef __linux__
#include <bsd/string.h>
#endif

#include "helper-unix.h"
#include "helper-all.h"
#include "helper-validate.h"

static array api_register_valid(void)
{
  array registermeta = ARRAY_DICT_INIT;
  ADD(registermeta, STRING_OBJ(cstring_copy_string("register")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("register a plugin")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("stze")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("none")));

  array registerfunc1args = ARRAY_DICT_INIT;
  ADD(registerfunc1args, INTEGER_OBJ(1234));
  ADD(registerfunc1args, STRING_OBJ(cstring_copy_string("func1 arg")));

  array registerfunc1 = ARRAY_DICT_INIT;
  ADD(registerfunc1, STRING_OBJ(cstring_copy_string("my function name")));
  ADD(registerfunc1, STRING_OBJ(cstring_copy_string("my function description")));
  ADD(registerfunc1, ARRAY_OBJ(registerfunc1args));

  array registerfunc2 = ARRAY_DICT_INIT;
  ADD(registerfunc2, STRING_OBJ(cstring_copy_string("my sec function name")));
  ADD(registerfunc2, STRING_OBJ(cstring_copy_string("my sec function description")));
  ADD(registerfunc2, ARRAY_OBJ(ARRAY_DICT_INIT));

  array registerfunctions = ARRAY_DICT_INIT;
  ADD(registerfunctions, ARRAY_OBJ(registerfunc1));
  ADD(registerfunctions, ARRAY_OBJ(registerfunc2));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(registermeta));
  ADD(request, ARRAY_OBJ(registerfunctions));

  return request;
}

static array api_register_wrong_args_size(void)
{
  array registermeta = ARRAY_DICT_INIT;
  ADD(registermeta, STRING_OBJ(cstring_copy_string("register")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("register a plugin")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("stze")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("none")));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(registermeta));

  return request;
}

static array api_register_wrong_args_type(void)
{
  array registerfunc1 = ARRAY_DICT_INIT;
  ADD(registerfunc1, STRING_OBJ(cstring_copy_string("my function name")));
  ADD(registerfunc1, STRING_OBJ(cstring_copy_string("my function description")));
  ADD(registerfunc1, ARRAY_OBJ(ARRAY_DICT_INIT));

  array registerfunc2 = ARRAY_DICT_INIT;
  ADD(registerfunc2, STRING_OBJ(cstring_copy_string("my sec function name")));
  ADD(registerfunc2, STRING_OBJ(cstring_copy_string("my sec function description")));
  ADD(registerfunc2, ARRAY_OBJ(ARRAY_DICT_INIT));

  array registerfunctions = ARRAY_DICT_INIT;
  ADD(registerfunctions, ARRAY_OBJ(registerfunc1));
  ADD(registerfunctions, ARRAY_OBJ(registerfunc2));

  array request = ARRAY_DICT_INIT;
  ADD(request, STRING_OBJ(cstring_copy_string("wrong")));
  ADD(request, ARRAY_OBJ(registerfunctions));

  return request;
}

static array api_register_wrong_meta_size(void)
{
  array registermeta = ARRAY_DICT_INIT;
  ADD(registermeta, STRING_OBJ(cstring_copy_string("register")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("register a plugin")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("stze")));

  array registerfunc1 = ARRAY_DICT_INIT;
  ADD(registerfunc1, STRING_OBJ(cstring_copy_string("my function name")));
  ADD(registerfunc1, STRING_OBJ(cstring_copy_string("my function description")));
  ADD(registerfunc1, ARRAY_OBJ(ARRAY_DICT_INIT));

  array registerfunc2 = ARRAY_DICT_INIT;
  ADD(registerfunc2, STRING_OBJ(cstring_copy_string("my sec function name")));
  ADD(registerfunc2, STRING_OBJ(cstring_copy_string("my sec function description")));
  ADD(registerfunc2, ARRAY_OBJ(ARRAY_DICT_INIT));

  array registerfunctions = ARRAY_DICT_INIT;
  ADD(registerfunctions, ARRAY_OBJ(registerfunc1));
  ADD(registerfunctions, ARRAY_OBJ(registerfunc2));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(registermeta));
  ADD(request, ARRAY_OBJ(registerfunctions));

  return request;
}

static array api_register_missing_function_description(void)
{
  array registermeta = ARRAY_DICT_INIT;
  ADD(registermeta, STRING_OBJ(cstring_copy_string("register")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("register a plugin")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("stze")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("none")));

  array registerfunc1 = ARRAY_DICT_INIT;
  ADD(registerfunc1, STRING_OBJ(cstring_copy_string("my function name")));
  ADD(registerfunc1, OBJECT_OBJ((object) OBJECT_INIT));
  ADD(registerfunc1, ARRAY_OBJ(ARRAY_DICT_INIT));

  array registerfunc2 = ARRAY_DICT_INIT;
  ADD(registerfunc2, STRING_OBJ(cstring_copy_string("my sec function name")));
  ADD(registerfunc2, STRING_OBJ(cstring_copy_string("my sec function description")));
  ADD(registerfunc2, ARRAY_OBJ(ARRAY_DICT_INIT));

  array registerfunctions = ARRAY_DICT_INIT;
  ADD(registerfunctions, ARRAY_OBJ(registerfunc1));
  ADD(registerfunctions, ARRAY_OBJ(registerfunc2));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(registermeta));
  ADD(request, ARRAY_OBJ(registerfunctions));

  return request;
}

static array api_register_empty_functions(void)
{
  array registermeta = ARRAY_DICT_INIT;
  ADD(registermeta, STRING_OBJ(cstring_copy_string("register")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("register a plugin")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("stze")));
  ADD(registermeta, STRING_OBJ(cstring_copy_string("none")));

  array registerfunctions = ARRAY_DICT_INIT;

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(registermeta));
  ADD(request, ARRAY_OBJ(registerfunctions));

  return request;
}

void functional_dispatch_handle_register(UNUSED(void **state))
{
  struct connection *con;
  array request;
  struct api_error error = ERROR_INIT;
  char pluginkey[PLUGINKEY_STRING_SIZE] = "012345789ABCDEFH";

  con = CALLOC(1, struct connection);
  con->closed = true;
  con->id = 1234;
  con->msgid = 4321;
  strlcpy(con->cc.pluginkeystring, pluginkey, PLUGINKEY_STRING_SIZE+1);
  assert_non_null(con);

  connect_and_create(con->cc.pluginkeystring);
  assert_int_equal(0, connection_init());

  connection_hashmap_put(con->id, con);

  /* a valid request has to trigger the corresponding response */
  request = api_register_valid();
  assert_false(error.isset);
  handle_register(con->id, 123, con->cc.pluginkeystring, request, &error);
  assert_false(error.isset);
  api_free_array(request);

  /* payload does not consist of two arrays, meta and func */
  request = api_register_wrong_args_size();
  assert_false(error.isset);
  handle_register(con->id, 123, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  /* object has wrong type */
  request = api_register_wrong_args_type();
  assert_false(error.isset);
  handle_register(con->id, 123, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  /* meta has wrong size */
  request = api_register_wrong_meta_size();
  assert_false(error.isset);
  handle_register(con->id, 123, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  /* no function description field should fail */
  request = api_register_missing_function_description();
  assert_false(error.isset);
  handle_register(con->id, 123, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  /* registering w/o function, should not work */
  request = api_register_empty_functions();
  assert_false(error.isset);
  handle_register(con->id, 123, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  connection_teardown();
  FREE(con);
  db_close();
}
