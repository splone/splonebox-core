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
#include "tweetnacl.h"
#include "rpc/sb-rpc.h"
#include "rpc/msgpack/sb-msgpack-rpc.h"

#include "helper-unix.h"
#include "helper-all.h"
#include "helper-validate.h"

void functional_dispatch_handle_register(UNUSED(void **state))
{
  connection_request_event_info info;
  struct message_request *request;
  array *meta, *functions, *func1, *func2, *args;
  struct api_error *err = MALLOC(struct api_error);
  char pluginkey[PLUGINKEY_STRING_SIZE] = "012345789ABCDEFH";

  string name = cstring_copy_string("register");
  string description = cstring_copy_string("register a plugin");
  string author = cstring_copy_string("test");
  string license = cstring_copy_string("none");

  info.request.msgid = 1;

  request = &info.request;
  assert_non_null(request);

  info.api_error = *err;
  assert_non_null(err);

  info.api_error.isset = false;

  info.con = CALLOC(1, struct connection);
  info.con->closed = true;
  info.con->id = 1234;
  strlcpy(info.con->cc.pluginkeystring, pluginkey, PLUGINKEY_STRING_SIZE+1);
  assert_non_null(info.con);

  connect_and_create(info.con->cc.pluginkeystring);
  assert_int_equal(0, connection_init());

  connection_hashmap_put(info.con->id, info.con);

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
  meta->size = 4;
  meta->obj = CALLOC(meta->size, struct message_object);
  meta->obj[0].type = OBJECT_TYPE_STR;
  meta->obj[0].data.string = name;
  meta->obj[1].type = OBJECT_TYPE_STR;
  meta->obj[1].data.string = description;
  meta->obj[2].type = OBJECT_TYPE_STR;
  meta->obj[2].data.string = author;
  meta->obj[3].type = OBJECT_TYPE_STR;
  meta->obj[3].data.string = license;

  functions = &request->params.obj[1].data.params;
  functions->size = 2;
  functions->obj = CALLOC(functions->size, struct message_object);
  functions->obj[0].type = OBJECT_TYPE_ARRAY;
  functions->obj[1].type = OBJECT_TYPE_ARRAY;

  func1 = &functions->obj[0].data.params;
  func1->size = 3;
  func1->obj = CALLOC(3, struct message_object);
  func1->obj[0].type = OBJECT_TYPE_STR;
  func1->obj[0].data.string = cstring_copy_string("my function name");
  func1->obj[1].type = OBJECT_TYPE_STR;
  func1->obj[1].data.string = cstring_copy_string("my function description");
  func1->obj[2].type = OBJECT_TYPE_ARRAY;
  func1->obj[2].data.params.size = 0;

  func2 = &functions->obj[1].data.params;
  func2->size = 3;
  func2->obj = CALLOC(3, struct message_object);
  func2->obj[0].type = OBJECT_TYPE_STR;
  func2->obj[0].data.string = cstring_copy_string("my snd function name");
  func2->obj[1].type = OBJECT_TYPE_STR;
  func2->obj[1].data.string = cstring_copy_string("my snd function description");
  func2->obj[2].type = OBJECT_TYPE_ARRAY;
  func2->obj[2].data.params.size = 0;

  /* a valid request has to trigger the corresponding response */
  assert_false(info.api_error.isset);
  expect_check(__wrap_crypto_write, &deserialized, validate_register_response, NULL);
  assert_int_equal(0, handle_register(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));

  /* payload does not consist of two arrays, meta and func */
  request->params.size = 1;
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_register(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;
  request->params.size = 2;

  /* object has wrong type */
  request->params.obj[0].type = OBJECT_TYPE_STR;
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_register(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;
  request->params.obj[0].type = OBJECT_TYPE_ARRAY;

  /* meta has wrong size */
  meta->size = 3;
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_register(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;
  meta->size = 4;

  /* plugin name is very long and contains same random bytes. since
   * there are no real constrains for the plugin name, this should work.
   */
  size_t len = 8096;
  unsigned char *buf = MALLOC_ARRAY(len, unsigned char);
  assert_non_null(buf);
  randombytes(buf, len);
  meta->obj[0].data.string.str = (char*) buf;
  meta->obj[0].data.string.length = len;
  assert_false(info.api_error.isset);
  expect_check(__wrap_crypto_write, &deserialized, validate_register_response, NULL);
  assert_int_equal(0, handle_register(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_false(info.api_error.isset);
  meta->obj[0].data.string = name;
  FREE(buf);

  /* no function description field should fail */
  func1->obj[1].type = OBJECT_TYPE_NIL;
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_register(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  func1->obj[1].type = OBJECT_TYPE_STR;
  info.api_error.isset = false;

  /* very long function description should work so far */
  string func_desc = func1->obj[1].data.string;
  len = 8096;
  buf = MALLOC_ARRAY(len, unsigned char);
  assert_non_null(buf);
  randombytes(buf, len);
  func1->obj[1].data.string.str = (char*) buf;
  func1->obj[1].data.string.length = len;
  assert_false(info.api_error.isset);
  expect_check(__wrap_crypto_write, &deserialized, validate_register_response, NULL);
  assert_int_equal(0, handle_register(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_false(info.api_error.isset);
  func1->obj[1].data.string = func_desc;
  FREE(buf);

  /* registering w/o function, should not work */
  functions->size = 0;
  assert_false(info.api_error.isset);
  assert_int_not_equal(0, handle_register(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_true(info.api_error.isset);
  assert_true(info.api_error.type == API_ERROR_TYPE_VALIDATION);
  info.api_error.isset = false;
  functions->size = 1;

  /* registering function with arguments, should work. the arguments
   * remain for the following tests. */
  args = &func1->obj[2].data.params;
  args->size = 2;
  args->obj = CALLOC(args->size, struct message_object);
  args->obj[0].type = OBJECT_TYPE_INT;
  args->obj[0].data.integer = 5;
  args->obj[1].type = OBJECT_TYPE_STR;
  args->obj[1].data.string = cstring_copy_string("foobar");
  assert_false(info.api_error.isset);
  expect_check(__wrap_crypto_write, &deserialized, validate_register_response, NULL);
  assert_int_equal(0, handle_register(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_false(info.api_error.isset);
  info.api_error.isset = false;

  /* registering only one function must work */
  functions->size = 1;
  assert_false(info.api_error.isset);
  expect_check(__wrap_crypto_write, &deserialized, validate_register_response, NULL);
  assert_int_equal(0, handle_register(info.con->id, &info.request, info.con->cc.pluginkeystring, &info.api_error));
  assert_false(info.api_error.isset);
  info.api_error.isset = false;
  functions->size = 2;

  free_params(request->params);
  connection_teardown();
  FREE(err);
  FREE(info.con);
  db_close();
}
