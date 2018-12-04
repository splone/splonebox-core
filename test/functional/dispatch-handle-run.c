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

#include "sb-common.h"
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

static array api_run_valid(struct plugin *plugin)
{
  array meta = ARRAY_DICT_INIT;
  ADD(meta, STRING_OBJ(cstring_copy_string(plugin->key.str)));
  ADD(meta, OBJECT_OBJ((object) OBJECT_INIT));

  array args = ARRAY_DICT_INIT;
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(meta));
  ADD(request, STRING_OBJ(cstring_copy_string(plugin->function->name.str)));
  ADD(request, ARRAY_OBJ(args));

  return request;
}

static array api_run_call_not_registered_function(struct plugin *plugin)
{
  array meta = ARRAY_DICT_INIT;
  ADD(meta, STRING_OBJ(cstring_copy_string(plugin->key.str)));
  ADD(meta, OBJECT_OBJ((object) OBJECT_INIT));

  array args = ARRAY_DICT_INIT;
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(meta));
  ADD(request, STRING_OBJ(cstring_copy_string("wrong")));
  ADD(request, ARRAY_OBJ(args));

  return request;
}

static array api_run_wrong_meta_type(struct plugin *plugin)
{
  array args = ARRAY_DICT_INIT;
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array request = ARRAY_DICT_INIT;
  ADD(request, STRING_OBJ(cstring_copy_string("wrong")));
  ADD(request, STRING_OBJ(cstring_copy_string(plugin->function->name.str)));
  ADD(request, ARRAY_OBJ(args));

  return request;
}

static array api_run_wrong_meta_size(struct plugin *plugin)
{
  array meta = ARRAY_DICT_INIT;
  ADD(meta, STRING_OBJ(cstring_copy_string(plugin->key.str)));

  array args = ARRAY_DICT_INIT;
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(meta));
  ADD(request, STRING_OBJ(cstring_copy_string(plugin->function->name.str)));
  ADD(request, ARRAY_OBJ(args));

  return request;
}

static array api_run_wrong_pluginkey_type(struct plugin *plugin)
{
  array meta = ARRAY_DICT_INIT;
  ADD(meta, OBJECT_OBJ((object) OBJECT_INIT));
  ADD(meta, OBJECT_OBJ((object) OBJECT_INIT));

  array args = ARRAY_DICT_INIT;
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[0].str)));
  ADD(args, STRING_OBJ(cstring_copy_string(plugin->function->args[1].str)));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(meta));
  ADD(request, STRING_OBJ(cstring_copy_string(plugin->function->name.str)));
  ADD(request, ARRAY_OBJ(args));

  return request;
}

void functional_dispatch_handle_run(UNUSED(void **state))
{
  struct plugin *plugin = helper_get_example_plugin();
  struct connection *con;
  struct api_error error = ERROR_INIT;
  array request;

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

  request = api_run_valid(plugin);
  handle_run(con->id, 123, con->cc.pluginkeystring, request, &error);
  assert_false(error.isset);
  api_free_array(request);

  /*
   * The following asserts verify, that the handle_run method cancels
   * as soon as illegitim run calls are processed. A API_ERROR must be
   * set in order inform the caller later on.
   */

  /* calling not registered function should fail */
  request = api_run_call_not_registered_function(plugin);
  handle_run(con->id, 123, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  /* meta object has wrong type */
  request = api_run_wrong_meta_type(plugin);
  assert_false(error.isset);
  handle_run(con->id, 123, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  /* meta has wrong size */
  request = api_run_wrong_meta_size(plugin);
  assert_false(error.isset);
  handle_run(con->id, 123, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  /* target plugin key has wrong type */
  request = api_run_wrong_pluginkey_type(plugin);
  assert_false(error.isset);
  handle_run(con->id, 123, con->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  assert_true(error.type == API_ERROR_TYPE_VALIDATION);
  error.isset = false;
  api_free_array(request);

  helper_free_plugin(plugin);
  connection_teardown();
  FREE(con);
  db_close();
}
