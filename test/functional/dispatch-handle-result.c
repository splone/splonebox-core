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
#include <bsd/string.h>

#include "helper-all.h"
#include "sb-common.h"
#include "rpc/sb-rpc.h"
#include "rpc/connection/connection.h"
#include "api/helpers.h"
#include "api/sb-api.h"
#ifdef __linux__
#include <bsd/string.h>
#endif

#include "helper-all.h"
#include "helper-unix.h"
#include "helper-validate.h"

static array api_result_valid(struct plugin * plugin)
{
  array meta = ARRAY_DICT_INIT;
  ADD(meta, UINTEGER_OBJ(plugin->callid));

  array args = ARRAY_DICT_INIT;
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(meta));
  ADD(request, ARRAY_OBJ(args));

  return request;
}

static array api_result_wrong_args_size_small(struct plugin * plugin)
{
  array meta = ARRAY_DICT_INIT;
  ADD(meta, UINTEGER_OBJ(plugin->callid));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(meta));

  return request;
}

static array api_result_wrong_args_size_big(struct plugin * plugin)
{
  array meta = ARRAY_DICT_INIT;
  ADD(meta, UINTEGER_OBJ(plugin->callid));

  array args = ARRAY_DICT_INIT;
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array wrong = ARRAY_DICT_INIT;

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(meta));
  ADD(request, ARRAY_OBJ(args));
  ADD(request, ARRAY_OBJ(wrong));

  return request;
}

static array api_result_wrong_meta_size(struct plugin * plugin)
{
  array meta = ARRAY_DICT_INIT;
  ADD(meta, UINTEGER_OBJ(plugin->callid));
  ADD(meta, STRING_OBJ(cstring_copy_string("wrong")));

  array args = ARRAY_DICT_INIT;
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(meta));
  ADD(request, ARRAY_OBJ(args));

  return request;
}

static array api_result_wrong_meta_type(struct plugin * plugin)
{
  array args = ARRAY_DICT_INIT;
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array request = ARRAY_DICT_INIT;
  ADD(request, STRING_OBJ(cstring_copy_string("wrong")));
  ADD(request, ARRAY_OBJ(args));

  return request;
}

static array api_result_wrong_meta_callid_type(struct plugin * plugin)
{
  array meta = ARRAY_DICT_INIT;
  ADD(meta, STRING_OBJ(cstring_copy_string("wrong")));

  array args = ARRAY_DICT_INIT;
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(meta));
  ADD(request, ARRAY_OBJ(args));

  return request;
}


void functional_dispatch_handle_result(UNUSED(void **state))
{
  struct api_error error = ERROR_INIT;
  array request = ARRAY_DICT_INIT;
  struct connection *con;

  /* create test plugin that is used for the tests */
  struct plugin *plugin = helper_get_example_plugin();

  con = CALLOC(1, struct connection);
  con->closed = true;
  con->id = (uint64_t) randommod(281474976710656LL);
  con->msgid = 4321;

  assert_non_null(con);

  strlcpy(con->cc.pluginkeystring, plugin->key.str, plugin->key.length+1);

  array registermeta = ARRAY_DICT_INIT;
  ADD(registermeta, STRING_OBJ(cstring_copy_string(plugin->name.str)));
  ADD(registermeta, STRING_OBJ(cstring_copy_string(plugin->description.str)));
  ADD(registermeta, STRING_OBJ(cstring_copy_string(plugin->author.str)));
  ADD(registermeta, STRING_OBJ(cstring_copy_string(plugin->license.str)));

  array registerarguments = ARRAY_DICT_INIT;
  ADD(registerarguments, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(registerarguments, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array registerfunc1 = ARRAY_DICT_INIT;
  ADD(registerfunc1, STRING_OBJ(cstring_copy_string(plugin->function->name.str)));
  ADD(registerfunc1, STRING_OBJ(cstring_copy_string(plugin->function->description.str)));
  ADD(registerfunc1, ARRAY_OBJ(registerarguments));

  array registerfunctions = ARRAY_DICT_INIT;
  ADD(registerfunctions, ARRAY_OBJ(registerfunc1));

  array registerrequest = ARRAY_DICT_INIT;
  ADD(registerrequest, ARRAY_OBJ(registermeta));
  ADD(registerrequest, ARRAY_OBJ(registerfunctions));

  connect_to_db();
  assert_int_equal(0, connection_init());

  error.isset = false;

  con->refcount++;

  connection_hashmap_put(con->id, con);
  pluginkeys_hashmap_put(con->cc.pluginkeystring, con->id);

  handle_register(con->id, 123, con->cc.pluginkeystring, registerrequest,
      &error);
  assert_false(error.isset);
  api_free_array(registerrequest);

  expect_check(__wrap_crypto_write, &deserialized, validate_run_request, plugin);

  // RUN API CALL
  array runmeta = ARRAY_DICT_INIT;
  ADD(runmeta, STRING_OBJ(cstring_copy_string(plugin->key.str)));
  ADD(runmeta, OBJECT_OBJ((object) OBJECT_INIT));

  array runargs = ARRAY_DICT_INIT;
  ADD(runargs, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(runargs, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array runrequest = ARRAY_DICT_INIT;
  ADD(runrequest, ARRAY_OBJ(runmeta));
  ADD(runrequest, STRING_OBJ(cstring_copy_string(plugin->function->name.str)));
  ADD(runrequest, ARRAY_OBJ(runargs));

  handle_run(con->id, 1234, con->cc.pluginkeystring, runrequest, &error);
  api_free_array(runrequest);

  expect_check(__wrap_crypto_write, &deserialized, validate_result_request, NULL);

  request = api_result_valid(plugin);
  handle_result(con->id, 1234, con->cc.pluginkeystring, request, &error);
  assert_false(error.isset);
  api_free_array(request);
  /*
   * The following asserts verify, that the handle_result method cancels
   * as soon as illegitim result calls are processed. A API_ERROR must be
   * set in order inform the caller later on.
   */

  /* size of payload too small */
  request = api_result_wrong_args_size_small(plugin);
  handle_result(con->id, 1234, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  /* size of payload too big */
  request = api_result_wrong_args_size_big(plugin);
  handle_result(con->id, 1234, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  /* wrong meta size */
  request = api_result_wrong_meta_size(plugin);
  handle_result(con->id, 1234, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  /* wrong meta type */
  request = api_result_wrong_meta_type(plugin);
  handle_result(con->id, 1234, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  /* wrong callid type */
  request = api_result_wrong_meta_callid_type(plugin);
  handle_result(con->id, 1234, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  helper_free_plugin(plugin);
  connection_teardown();
  FREE(con);
  db_close();
}
