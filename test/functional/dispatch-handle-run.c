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

#define KEY "vBXBg3Wkq3ESULkYWtijxfS5UvBpWb-2mZHpKAKpyRuTmvdy4WR7cTJqz-vi2BA2"
#define FUNC "test_function"
#define ARG1 "test arg1"
#define ARG2 "test arg2"

static string apikey;
static string pluginname;
static string description;
static string author;
static string license;
static struct message_request *register_request;


int validate_run_response(const unsigned long data1,
  UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  struct message_object response;
  array params;

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

  free_params(params);

  return (1);
}

int validate_run_request(const unsigned long data1,
  UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  struct message_object meta, request, func, args;
  array params;

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
  will_return(__wrap_loop_wait_for_response, OBJECT_TYPE_UINT);
  will_return(__wrap_loop_wait_for_response, callid);
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

  free_params(params);

  return (1);
}

static void register_test_function(void)
{
  connection_request_event_info info;
  array *meta, *functions, *func1, *args;
  struct api_error err = ERROR_INIT;
  const char *key = "vBXBg3Wkq3ESULkYWtijxfS5UvBpWb-2mZHpKAKpyRuTmvdy4WR7cTJqz-vi2BA2";

  apikey = cstring_copy_string(key);
  pluginname = cstring_copy_string("plugin name");
  description = cstring_copy_string("register a plugin");
  author = cstring_copy_string("test");
  license = cstring_copy_string("none");

  info.request = MALLOC(struct message_request);
  info.request->msgid = 1;
  register_request = info.request;
  assert_non_null(register_request);

  info.api_error = err;

  info.con = MALLOC(struct connection);
  info.con->closed = true;
  info.con->msgid = 1;
  assert_non_null(info.con);

  connect_and_create(apikey);
  assert_int_equal(0, connection_init());

  /* first level arrays:
   *
   * [meta] args->obj[0]
   * [functions] args->obj[1]
   */
  register_request->params.size = 2;
  register_request->params.obj = CALLOC(2, struct message_object);
  register_request->params.obj[0].type = OBJECT_TYPE_ARRAY;
  register_request->params.obj[1].type = OBJECT_TYPE_ARRAY;

  /* meta array:
   *
   * [name]
   * [desciption]
   * [author]
   * [license]
   */
  meta = &register_request->params.obj[0].data.params;
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

  functions = &register_request->params.obj[1].data.params;
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
  expect_check(__wrap_crypto_write, &deserialized, validate_register_response, NULL);
  assert_int_equal(0, handle_register(&info));
  assert_false(info.api_error.isset);
}

static struct message_request * run_request_helper(string key,
    string function_name, message_object_type metaarraytype,
    message_object_type functionnametype, message_object_type argstype,
    size_t metasize, message_object_type metaclientidtype,
    message_object_type metacallidtype)
{
  array argsarray = ARRAY_INIT;
  array *meta;
  struct message_request *rr;

  rr = MALLOC(struct message_request);
  assert_non_null(rr);

  rr->msgid = 1;

  rr->params.size = 3;
  rr->params.obj = CALLOC(rr->params.size, struct message_object);
  rr->params.obj[0].type = metaarraytype;
  rr->params.obj[1].type = functionnametype;
  rr->params.obj[2].type = argstype;

  meta = &rr->params.obj[0].data.params;
  meta->size = metasize;
  meta->obj = CALLOC(meta->size, struct message_object);
  meta->obj[0].type = metaclientidtype;
  meta->obj[0].data.string = cstring_copy_string(key.str);
  meta->obj[1].type = metacallidtype;

  rr->params.obj[1].data.string = cstring_copy_string(function_name.str);

  argsarray.size = 2;
  argsarray.obj = CALLOC(argsarray.size, struct message_object);
  argsarray.obj[0].type = OBJECT_TYPE_STR; /* first argument */
  argsarray.obj[0].data.string = cstring_copy_string(ARG1);
  argsarray.obj[1].type = OBJECT_TYPE_STR; /* second argument */
  argsarray.obj[1].data.string = cstring_copy_string(ARG2);

  rr->params.obj[2].data.params = argsarray;

  return (rr);
}

void functional_dispatch_handle_run(UNUSED(void **state))
{
  connection_request_event_info info;
  struct api_error err = ERROR_INIT;
  string key, functionname, invalidfunctionname;

  register_test_function();

  info.api_error = err;
  assert_false(info.api_error.isset);
  info.con = MALLOC(struct connection);
  info.con->closed = true;
  assert_non_null(info.con);

  key = cstring_copy_string(KEY);
  functionname = cstring_copy_string(FUNC);

  expect_check(__wrap_crypto_write, &deserialized, validate_run_request, NULL);
  expect_check(__wrap_crypto_write, &deserialized, validate_run_response, NULL);

  /*
   * The following asserts verifies a legitim run call. In detail, it
   * verifies, that the forwarded run request by the server is of proper
   * format.
   * */
  info.request = run_request_helper(key, functionname, OBJECT_TYPE_ARRAY,
      OBJECT_TYPE_STR, OBJECT_TYPE_ARRAY, 2, OBJECT_TYPE_STR, OBJECT_TYPE_NIL);

  assert_int_equal(0, handle_run(&info));

  free_params(info.request->params);
  FREE(info.request);

  /*
   * The following asserts verify, that the handle_run method cancels
   * as soon as illegitim run calls are processed. A API_ERROR must be
   * set in order inform the caller later on.
   */

  /* calling not registered function should fail */
  invalidfunctionname = cstring_copy_string("invalid func name");
  info.request = run_request_helper(key, invalidfunctionname, OBJECT_TYPE_ARRAY,
      OBJECT_TYPE_STR, OBJECT_TYPE_ARRAY, 2, OBJECT_TYPE_STR, OBJECT_TYPE_NIL);

  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(&info));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;

  free_params(info.request->params);
  FREE(info.request);

  /* object has wrong type */
  info.request = run_request_helper(key, functionname, OBJECT_TYPE_STR,
      OBJECT_TYPE_STR, OBJECT_TYPE_ARRAY, 2, OBJECT_TYPE_STR, OBJECT_TYPE_NIL);
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(&info));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;

  /* need to free key string manually, because type is wrong */
  free_string(info.request->params.obj[0].data.params.obj[0].data.string);
  free_params(info.request->params);
  FREE(info.request);

  /* meta has wrong size */
  info.request = run_request_helper(key, functionname, OBJECT_TYPE_ARRAY,
      OBJECT_TYPE_STR, OBJECT_TYPE_ARRAY, 3, OBJECT_TYPE_STR, OBJECT_TYPE_NIL);
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(&info));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;

  free_params(info.request->params);
  FREE(info.request);

  /* client2_id has wrong type */
  info.request = run_request_helper(key, functionname, OBJECT_TYPE_ARRAY,
      OBJECT_TYPE_STR, OBJECT_TYPE_ARRAY, 2, OBJECT_TYPE_BIN, OBJECT_TYPE_NIL);

  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(&info));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;

  free_params(info.request->params);
  FREE(info.request);

  /* call_id has wrong type */
  info.request = run_request_helper(key, functionname, OBJECT_TYPE_ARRAY,
      OBJECT_TYPE_STR, OBJECT_TYPE_ARRAY, 2, OBJECT_TYPE_BIN, OBJECT_TYPE_BIN);

  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_run(&info));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);

  free_params(info.request->params);
  FREE(info.request);

  free_string(invalidfunctionname);
  free_string(key);
  free_string(functionname);
  free_params(register_request->params);
  FREE(register_request);
  FREE(info.con);
  connection_teardown();
  db_close();
}
