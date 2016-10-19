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
#include "rpc/msgpack/sb-msgpack-rpc.h"
#include "rpc/sb-rpc.h"

#include "helper-unix.h"
#include "helper-all.h"
#include "helper-validate.h"

static struct plugin *prepare_test(connection_request_event_info *info)
{
  struct api_error err = ERROR_INIT;
  info->api_error = err;

  /* create test plugin that is used for the tests */
  struct plugin *plugin = helper_get_example_plugin();
  helper_register_plugin(plugin);

  /* establish fake connection to plugin */
  info->con = CALLOC(1, struct connection);
  info->con->closed = true;
  info->con->id = 12345;
  connection_hashmap_put(info->con->id, info->con);
  strlcpy(info->con->cc.pluginkeystring, plugin->key.str, plugin->key.length+1);
  assert_non_null(info->con);

  return plugin;
}

void functional_dispatch_handle_run(UNUSED(void **state))
{
  connection_request_event_info info;
  struct plugin *plugin;

  plugin = prepare_test(&info);

  expect_check(__wrap_crypto_write, &deserialized, validate_run_request, plugin);
  expect_check(__wrap_crypto_write, &deserialized, validate_run_response, plugin);

  /*
   * The following asserts verifies a legitim run call. In detail, it
   * verifies, that the forwarded run request by the server is of proper
   * format.
   * */
  helper_build_run_request(&info.request, plugin
    ,OBJECT_TYPE_ARRAY  /* meta array type */
    ,2                  /* meta size */
    ,OBJECT_TYPE_STR    /* target plugin key */
    ,OBJECT_TYPE_NIL    /* call id type */
    ,OBJECT_TYPE_STR    /* function name */
    ,OBJECT_TYPE_ARRAY  /* arguments */
  );

  //nfo.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error

  assert_int_equal(0, handle_run(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_false(info.api_error.isset);

  /*
   * The following asserts verify, that the handle_run method cancels
   * as soon as illegitim run calls are processed. A API_ERROR must be
   * set in order inform the caller later on.
   */

  /* calling not registered function should fail */
  helper_request_set_function_name(&info.request, OBJECT_TYPE_STR,
    "invalid funcion name");

  assert_int_not_equal(0, handle_run(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;

  /* reset to correct request */
  helper_request_set_function_name(&info.request, OBJECT_TYPE_STR,
    plugin->function->name.str);

  /* meta object has wrong type */
  helper_request_set_meta_size(&info.request, OBJECT_TYPE_STR, 2);

  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;

  /* reset to correct request */
  helper_request_set_meta_size(&info.request, OBJECT_TYPE_ARRAY, 2);

  /* meta has wrong size */
  helper_request_set_meta_size(&info.request, OBJECT_TYPE_ARRAY, 3);

  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;

  /* reset to correct request */
  helper_request_set_meta_size(&info.request, OBJECT_TYPE_ARRAY, 2);

  /* target plugin key has wrong type */
  helper_request_set_pluginkey_type(&info.request, OBJECT_TYPE_ARRAY, plugin->key.str);

  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;

  /* reset to correct request */
  helper_request_set_pluginkey_type(&info.request, OBJECT_TYPE_STR, plugin->key.str);
  array *meta = &info.request.params.obj[0].data.params;
  free_string(meta->obj[0].data.string);

  /* call_id has wrong type */
  helper_request_set_callid(&info.request, OBJECT_TYPE_BIN);

  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);

  /* reset to correct request */
  helper_request_set_callid(&info.request, OBJECT_TYPE_NIL);

  free_params(info.request.params);
  helper_free_plugin(plugin);
  connection_teardown();
  db_close();
}
