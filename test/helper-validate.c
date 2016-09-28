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

#include "rpc/db/sb-db.h"
#include "rpc/msgpack/sb-msgpack-rpc.h"

#include "helper-unix.h"
#include "helper-all.h"

int validate_register_response(const unsigned long data1,
    UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  struct message_object request;
  array params;

  wrap_crypto_write = true;

  assert_int_equal(0, unpack_params(deserialized, &params));

  /* msgpack request needs to be 1 */
  assert_true(params.obj[0].type == OBJECT_TYPE_UINT);
  assert_int_equal(1, params.obj[0].data.uinteger);

  /* msg id which is random */
  assert_true(params.obj[1].type == OBJECT_TYPE_UINT);

  /* next field is NIL */
  assert_true(params.obj[2].type == OBJECT_TYPE_NIL);

  /* followed by an empty array */
  request = params.obj[3];
  assert_true(request.type == OBJECT_TYPE_ARRAY);
  assert_int_equal(0, request.data.params.size);

  free_params(params);

  return (1);
}

int validate_run_request(const unsigned long data1,
  UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  struct plugin *p = (struct plugin *) data2;
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
  p->callid = callid;

  /* the function to call on client2 side */
  func = request.data.params.obj[1];
  assert_true(func.type == OBJECT_TYPE_STR);
  assert_string_equal(func.data.string.str, p->function->name.str);

  /* the function arguments to pass to client2 side */
  assert_true(request.data.params.obj[2].type == OBJECT_TYPE_ARRAY);
  assert_int_equal(2, request.data.params.obj[2].data.params.size);

  args = request.data.params.obj[2];
  assert_true(args.data.params.obj[0].type == OBJECT_TYPE_STR);
  assert_string_equal(args.data.params.obj[0].data.string.str, p->function->args[0].str);

  assert_true(args.data.params.obj[1].type == OBJECT_TYPE_STR);
  assert_string_equal(args.data.params.obj[1].data.string.str, p->function->args[1].str);

  free_params(params);

  return (1);
}


int validate_run_response(const unsigned long data1,
  UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  struct message_object response;
  struct plugin *p = (struct plugin *) data2;
  array params;

  wrap_crypto_write = true;

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
  //TODO validate callid against plugin->callid
  if (p) {
    assert_true(response.data.params.obj[0].data.uinteger == p->callid);
  }

  free_params(params);

  return (1);
}

int validate_result_request(const unsigned long data1,
  UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  struct message_object meta, request;
  array params;

  assert_int_equal(0, unpack_params(deserialized, &params));

  /* msgpack request needs to be 0 */
  assert_true(params.obj[0].type == OBJECT_TYPE_UINT);
  assert_int_equal(0, params.obj[0].data.uinteger);

  /* msg id which is random */
  assert_true(params.obj[1].type == OBJECT_TYPE_UINT);

  /* run call */
  assert_true(params.obj[2].type == OBJECT_TYPE_STR);
  assert_string_equal(params.obj[2].data.string.str, "result");

  /* first level arrays:
   *
   * [meta] args->obj[0]
   * [functions] args->obj[1]
   */
  request = params.obj[3];
  assert_true(request.type == OBJECT_TYPE_ARRAY);
  assert_int_equal(2, request.data.params.size);

  meta = request.data.params.obj[0];
  assert_true(meta.type == OBJECT_TYPE_ARRAY);
  assert_int_equal(1, meta.data.params.size);

  /* client2 id should be nil */
  assert_true(meta.data.params.obj[0].type == OBJECT_TYPE_UINT);

  /* since the callid must be forwarded to the client1, the original
   * sender of the rpc call, we need to push the callid on the cmocka test
   * stack. this allows verifying of it later on */
  uint64_t callid = meta.data.params.obj[0].data.uinteger;

  will_return(__wrap_loop_wait_for_response, OBJECT_TYPE_UINT);
  will_return(__wrap_loop_wait_for_response, callid);
  expect_value(validate_result_response, response.data.params.obj[0].data.uinteger, callid);

  free_params(params);

  return (1);
}

int validate_result_response(const unsigned long data1,
  UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  struct message_object response;
  array params;

  wrap_crypto_write = true;

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


