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
#include "api/helpers.h"
#include "rpc/msgpack/helpers.h"

#include "helper-unix.h"
#include "helper-all.h"


int validate_run_request(const unsigned long data1,
  UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  struct plugin *p = (struct plugin *) data2;

  array message;
  object request, meta, func, args;

  msgpack_rpc_to_array(deserialized, &message);

  /* msgpack request needs to be 0 */
  assert_true(message.items[0].type == OBJECT_TYPE_UINT);
  assert_int_equal(0, message.items[0].data.uinteger);

  /* msg id which is random */
  assert_true(message.items[1].type == OBJECT_TYPE_UINT);

  /* run call */
  assert_true(message.items[2].type == OBJECT_TYPE_STR);
  assert_string_equal(message.items[2].data.string.str, "run");

  /* first level arrays:
   *
   * [meta] args->obj[0]
   * [functions] args->obj[1]
   */
  request = message.items[3];
  assert_true(request.type == OBJECT_TYPE_ARRAY);
  assert_int_equal(3, request.data.array.size);

  meta = request.data.array.items[0];
  assert_true(meta.type == OBJECT_TYPE_ARRAY);
  assert_int_equal(2, meta.data.array.size);

  /* client2 id should be nil */
  assert_true(meta.data.array.items[0].type == OBJECT_TYPE_NIL);

  /* verify that the server sent a proper callid */
  assert_true(meta.data.array.items[1].type == OBJECT_TYPE_UINT);

  /* since the callid must be forwarded to the client1, the original
   * sender of the rpc call, we need to push the callid on the cmocka test
   * stack. this allows verifying of it later on */
  uint64_t callid = meta.data.array.items[1].data.uinteger;
  will_return(__wrap_loop_process_events_until, OBJECT_TYPE_UINT);
  will_return(__wrap_loop_process_events_until, callid);

  p->callid = callid;

  /* the function to call on client2 side */
  func = request.data.array.items[1];
  assert_true(func.type == OBJECT_TYPE_STR);
  assert_string_equal(func.data.string.str, p->function->name.str);

  /* the function arguments to pass to client2 side */
  assert_true(request.data.array.items[2].type == OBJECT_TYPE_ARRAY);
  assert_int_equal(2, request.data.array.items[2].data.array.size);

  args = request.data.array.items[2];
  assert_true(args.data.array.items[0].type == OBJECT_TYPE_STR);
  assert_string_equal(args.data.array.items[0].data.string.str, p->function->args[0].str);

  assert_true(args.data.array.items[1].type == OBJECT_TYPE_STR);
  assert_string_equal(args.data.array.items[1].data.string.str, p->function->args[1].str);

  api_free_array(message);

  return (1);
}


int validate_result_request(const unsigned long data1,
  UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  array message;
  object meta, request;

  msgpack_rpc_to_array(deserialized, &message);

  /* msgpack request needs to be 0 */
  assert_true(message.items[0].type == OBJECT_TYPE_UINT);
  assert_int_equal(0, message.items[0].data.uinteger);

  /* msg id which is random */
  assert_true(message.items[1].type == OBJECT_TYPE_UINT);

  /* run call */
  assert_true(message.items[2].type == OBJECT_TYPE_STR);
  assert_string_equal(message.items[2].data.string.str, "result");

  /* first level arrays:
   *
   * [meta] args->obj[0]
   * [functions] args->obj[1]
   */
  request = message.items[3];
  assert_true(request.type == OBJECT_TYPE_ARRAY);
  assert_int_equal(2, request.data.array.size);

  meta = request.data.array.items[0];
  assert_true(meta.type == OBJECT_TYPE_ARRAY);
  assert_int_equal(1, meta.data.array.size);

  /* client2 id should be nil */
  assert_true(meta.data.array.items[0].type == OBJECT_TYPE_UINT);

  /* since the callid must be forwarded to the client1, the original
   * sender of the rpc call, we need to push the callid on the cmocka test
   * stack. this allows verifying of it later on */
  uint64_t callid = meta.data.array.items[0].data.uinteger;

  will_return(__wrap_loop_process_events_until, OBJECT_TYPE_UINT);
  will_return(__wrap_loop_process_events_until, callid);

  api_free_array(message);

  return (1);
}
