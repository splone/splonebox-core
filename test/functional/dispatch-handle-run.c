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

#include "helper-all.h"
#include "sb-common.h"
#include "rpc/msgpack/sb-msgpack-rpc.h"
#include "rpc/sb-rpc.h"
#include "helper-unix.h"
#include "wrappers.h"

#define KEY "vBXBg3Wkq3ESULkYWtijxfS5UvBpWb-2mZHpKAKpyRuTmvdy4WR7cTJqz-vi2BA2"
#define FUNC "test_function"
#define ARG1 "test arg1"
#define ARG2 "test arg2"

int validate_run_response(const unsigned long data1,
  UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  struct message_object response;
  struct message_params_object params;

  assert_int_equal(0, unpack_params(deserialized, &params));

  /* msgpack response needs to be 1 */
  assert_true(params.obj[0].type == OBJECT_TYPE_UINT);
  assert_int_equal(1, params.obj[0].data.uinteger);

  /* msg id which is random */
  assert_true(params.obj[1].type == OBJECT_TYPE_UINT);

  /* method should be NIL */
  assert_true(params.obj[2].type == OBJECT_TYPE_NIL);

  /* first level arrays:
   *
   * [meta] args->obj[0]
   */
  response = params.obj[3];
  assert_true(response.type == OBJECT_TYPE_ARRAY);
  assert_int_equal(1, response.data.params.size);

  /* the server must send a call id, store the callid for further
   * validation */
  assert_true(response.data.params.obj[0].type == OBJECT_TYPE_UINT);
  check_expected(response.data.params.obj[0].data.uinteger);

  return (1);
}

int validate_run_request(const unsigned long data1,
  UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  struct message_object meta, request, func, args;
  struct message_params_object params;

  assert_int_equal(0, unpack_params(deserialized, &params));

  /* msgpack request needs to be 0 */
  assert_true(params.obj[0].type == OBJECT_TYPE_UINT);
  assert_int_equal(0, params.obj[0].data.uinteger);

  /* msg id which is random */
  assert_true(params.obj[1].type == OBJECT_TYPE_UINT);

  /* run call */
  assert_true(params.obj[2].type == OBJECT_TYPE_STR);
  assert_string_equal(params.obj[2].data.string.str, "run");

  /* first level arrays:
   *
   * [meta] args->obj[0]
   * [functions] args->obj[1]
   */
  request = params.obj[3];
  assert_true(request.type == OBJECT_TYPE_ARRAY);
  assert_int_equal(3, request.data.params.size);

  meta = request.data.params.obj[0];
  assert_true(meta.type == OBJECT_TYPE_ARRAY);
  assert_int_equal(2, meta.data.params.size);

  /* client2 id should be nil */
  assert_true(meta.data.params.obj[0].type == OBJECT_TYPE_NIL);

  /* verify that the server sent a proper callid */
  assert_true(meta.data.params.obj[1].type == OBJECT_TYPE_UINT);

  /* since the callid must be forwarded to the client1, the original
   * sender of the rpc call, we need to push the callid on the cmocka test
   * stack. this allows verifying of it later on */
  uint64_t callid = meta.data.params.obj[1].data.uinteger;
  will_return(__wrap_connection_wait_for_response, OBJECT_TYPE_UINT);
  will_return(__wrap_connection_wait_for_response, callid);
  expect_value(validate_run_response, response.data.params.obj[0].data.uinteger, callid);

  /* the function to call on client2 side */
  func = request.data.params.obj[1];
  assert_true(func.type == OBJECT_TYPE_STR);
  assert_string_equal(func.data.string.str, FUNC);

  /* the function arguments to pass to client2 side */
  assert_true(request.data.params.obj[2].type == OBJECT_TYPE_ARRAY);
  assert_int_equal(2, request.data.params.obj[2].data.params.size);

  args = request.data.params.obj[2];
  assert_true(args.data.params.obj[0].type == OBJECT_TYPE_STR);
  assert_string_equal(args.data.params.obj[0].data.string.str, ARG1);

  assert_true(args.data.params.obj[1].type == OBJECT_TYPE_STR);
  assert_string_equal(args.data.params.obj[1].data.string.str, ARG2);

  return (1);
}

static void register_test_function(void)
{
  connection_request_event_info info;
  struct message_request *request;
  struct message_params_object *meta, *functions, *func1, *args;
  struct api_error *err = MALLOC(struct api_error);

  string apikey = cstring_copy_string(KEY);
  string pluginname = cstring_copy_string("plugin name");
  string description = cstring_copy_string("register a plugin");
  string author = cstring_copy_string("test");
  string license = cstring_copy_string("none");

  info.request = MALLOC(struct message_request);
  request = info.request;
  assert_non_null(request);

  info.api_error = *err;
  assert_non_null(err);

  info.con = MALLOC(struct connection);
  assert_non_null(info.con);

  connect_and_create(apikey);
  assert_int_equal(0, connection_init());

  /* first level arrays:
   *
   * [meta] args->obj[0]
   * [functions] args->obj[1]
   */
  request->params.size = 2;
  request->params.obj = CALLOC(2, struct message_object);
  request->params.obj[0].type = OBJECT_TYPE_ARRAY;
  request->params.obj[1].type = OBJECT_TYPE_ARRAY;

  /* meta array:
   *
   * [name]
   * [desciption]
   * [author]
   * [license]
   */
  meta = &request->params.obj[0].data.params;
  meta->size = 5;
  meta->obj = CALLOC(5, struct message_object);
  meta->obj[0].type = OBJECT_TYPE_STR;
  meta->obj[0].data.string = apikey;
  meta->obj[1].type = OBJECT_TYPE_STR;
  meta->obj[1].data.string = pluginname;
  meta->obj[2].type = OBJECT_TYPE_STR;
  meta->obj[2].data.string = description;
  meta->obj[3].type = OBJECT_TYPE_STR;
  meta->obj[3].data.string = author;
  meta->obj[4].type = OBJECT_TYPE_STR;
  meta->obj[4].data.string = license;

  functions = &request->params.obj[1].data.params;
  functions->size = 1;
  functions->obj = CALLOC(functions->size, struct message_object);
  functions->obj[0].type = OBJECT_TYPE_ARRAY;

  func1 = &functions->obj[0].data.params;
  func1->size = 3;
  func1->obj = CALLOC(3, struct message_object);
  func1->obj[0].type = OBJECT_TYPE_STR;
  func1->obj[0].data.string = cstring_copy_string(FUNC);
  func1->obj[1].type = OBJECT_TYPE_STR;
  func1->obj[1].data.string = cstring_copy_string("my function description");
  func1->obj[2].type = OBJECT_TYPE_ARRAY;

  /* function arguments */
  args = &func1->obj[2].data.params;
  args->size = 2;
  args->obj = CALLOC(2, struct message_object);
  args->obj[0].type = OBJECT_TYPE_STR;
  args->obj[0].data.string = cstring_copy_string(ARG1);
  args->obj[1].type = OBJECT_TYPE_STR;
  args->obj[1].data.string = cstring_copy_string(ARG2);

  /* before running function, it must be registered successfully */
  info.api_error.isset = false;
  expect_check(__wrap_outputstream_write, &deserialized, validate_register_response, NULL);
  assert_int_equal(0, handle_register(&info));
  assert_false(info.api_error.isset);

  free_params(request->params);
  FREE(info.con);
  FREE(request);
  FREE(err);
}

void functional_dispatch_handle_run(UNUSED(void **state))
{
  connection_request_event_info info;
  struct message_request *request;
  struct message_params_object *meta, *args;
  struct api_error *err = MALLOC(struct api_error);

  /* disabled wrapping of used functions */
  WRAP_API_RUN = false;
  WRAP_CONNECTION_SEND_REQUEST = false;
  WRAP_API_RUN_RESPONSE = false;
  WRAP_CONNECTION_SEND_RESPONSE = false;
  WRAP_RANDOMMOD = false;

  register_test_function();

  info.request = MALLOC(struct message_request);
  request = info.request;
  assert_non_null(request);

  info.api_error = *err;
  assert_non_null(err);

  info.con = MALLOC(struct connection);
  assert_non_null(info.con);

  request->params.size = 3;
  request->params.obj = CALLOC(request->params.size, struct message_object);
  request->params.obj[0].type = OBJECT_TYPE_ARRAY;
  request->params.obj[1].type = OBJECT_TYPE_STR;
  request->params.obj[2].type = OBJECT_TYPE_ARRAY;

  meta = &request->params.obj[0].data.params;
  meta->size = 2;
  meta->obj = CALLOC(meta->size, struct message_object);
  meta->obj[0].type = OBJECT_TYPE_STR;
  meta->obj[0].data.string = cstring_copy_string(KEY);
  meta->obj[1].type = OBJECT_TYPE_NIL;

  /* function name to call */
  request->params.obj[1].data.string = cstring_copy_string(FUNC);

  /* list of arguments */
  args = &request->params.obj[2].data.params;
  args->size = 2;
  args->obj = CALLOC(args->size, struct message_object);
  args->obj[0].type = OBJECT_TYPE_STR; /* first argument */
  args->obj[0].data.string = cstring_copy_string(ARG1);
  args->obj[1].type = OBJECT_TYPE_STR; /* second argument */
  args->obj[1].data.string = cstring_copy_string(ARG2);

  /*
   * The following asserts verifies a legitim run call. In detail, it
   * verifies, that the forwarded run request by the server is of proper
   * format.
   * */
  assert_false(info.api_error.isset);
  expect_check(__wrap_outputstream_write, &deserialized,
    validate_run_request, NULL);
   expect_check(__wrap_outputstream_write, &deserialized,
    validate_run_response, NULL);
  assert_int_equal(0, handle_run(&info));

  /*
   * The following asserts verify, that the handle_run method cancels
   * as soon as illegitim run calls are processed. A API_ERROR must be
   * set in order inform the caller later on.
   */

  /* calling not registered function should fail */
  string function_name = request->params.obj[1].data.string;
  string invalid_function_name = cstring_copy_string("invalid func name");
  request->params.obj[1].data.string = invalid_function_name;
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(&info));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  free_string(invalid_function_name);
  request->params.obj[1].data.string = function_name;
  info.api_error.isset = false;

  /* object has wrong type */
  request->params.obj[0].type = OBJECT_TYPE_STR;
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(&info));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  request->params.obj[0].type = OBJECT_TYPE_ARRAY;
  info.api_error.isset = false;

  /* meta has wrong size */
  meta->size = 3;
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(&info));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  meta->size = 2;
  info.api_error.isset = false;

  /* client2_id has wrong type */
  meta->obj[0].type = OBJECT_TYPE_BIN;
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(&info));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  meta->obj[0].type = OBJECT_TYPE_STR;
  info.api_error.isset = false;

  /* call_id has wrong type */
  meta->obj[1].type = OBJECT_TYPE_BIN;
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(&info));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  meta->obj[1].type = OBJECT_TYPE_NIL;
  info.api_error.isset = false;

  free_params(request->params);
  FREE(info.con);
  FREE(request);
  FREE(err);

}
