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
#include "rpc/sb-rpc.h"
#include "api/helpers.h"
#include "api/sb-api.h"
#ifdef __linux__
#include <bsd/string.h>
#endif

#include "helper-unix.h"
#include "helper-all.h"
#include "helper-validate.h"

static array api_broadcast_valid(void)
{
  array args = ARRAY_DICT_INIT;
  ADD(args, STRING_OBJ(cstring_copy_string("abc")));
  ADD(args, STRING_OBJ(cstring_copy_string("def")));

  array request = ARRAY_DICT_INIT;
  ADD(request, STRING_OBJ(cstring_copy_string("testname")));
  ADD(request, ARRAY_OBJ(args));

  return request;
}

static array api_subscribe_valid(void)
{
  array request = ARRAY_DICT_INIT;
  ADD(request, STRING_OBJ(cstring_copy_string("testname")));

  return request;
}

int validate_broadcast(const unsigned long data1,
  UNUSED(const unsigned long data2))
{
  struct msgpack_object *deserialized = (struct msgpack_object *) data1;
  array message;

  msgpack_rpc_to_array(deserialized, &message);

  assert_true(message.items[0].type == OBJECT_TYPE_UINT);
  assert_int_equal(2, message.items[0].data.uinteger);

  assert_true(message.items[1].type == OBJECT_TYPE_STR);
  assert_string_equal(message.items[1].data.string.str, "testname");

  assert_true(message.items[2].type == OBJECT_TYPE_ARRAY);
  assert_int_equal(2, message.items[2].data.array.size);

  assert_true(message.items[2].data.array.items[0].type == OBJECT_TYPE_STR);
  assert_string_equal(message.items[2].data.array.items[0].data.string.str,
      "abc");

  assert_true(message.items[2].data.array.items[1].type == OBJECT_TYPE_STR);
  assert_string_equal(message.items[2].data.array.items[1].data.string.str,
      "def");

  api_free_array(message);

  return 1;
}


void functional_dispatch_handle_broadcast(UNUSED(void **state))
{
  struct plugin *plugin = helper_get_example_plugin();
  struct connection *con1;
  struct connection *con2;
  struct api_error error = ERROR_INIT;
  array request;

  con1 = CALLOC(1, struct connection);
  con1->id = (uint64_t) randommod(281474976710656LL);
  con1->msgid = 4321;
  con1->subscribed_events = hashmap_new(cstr_t, ptr_t)();

  con2 = CALLOC(1, struct connection);
  con2->id = (uint64_t) randommod(281474976710656LL);
  con2->msgid = 4321;
  con2->subscribed_events = hashmap_new(cstr_t, ptr_t)();

  assert_non_null(con1);
  assert_non_null(con2);

  strlcpy(con1->cc.pluginkeystring, "ABCD", sizeof("ABCD") - 1);
  strlcpy(con2->cc.pluginkeystring, "EFGH", sizeof("EFGH") - 1);

  connect_to_db();
  assert_int_equal(0, connection_init());

  con1->refcount++;
  con2->refcount++;

  connection_hashmap_put(con1->id, con1);
  connection_hashmap_put(con2->id, con2);
  pluginkeys_hashmap_put(con1->cc.pluginkeystring, con1->id);
  pluginkeys_hashmap_put(con2->cc.pluginkeystring, con2->id);

  expect_check(__wrap_crypto_write, &deserialized, validate_broadcast, NULL);
  expect_check(__wrap_crypto_write, &deserialized, validate_broadcast, NULL);

  request = api_subscribe_valid();
  handle_subscribe(con1->id, 123, con1->cc.pluginkeystring, request, &error);
  assert_false(error.isset);
  api_free_array(request);

  request = api_subscribe_valid();
  handle_subscribe(con2->id, 123, con2->cc.pluginkeystring, request, &error);
  assert_false(error.isset);
  api_free_array(request);

  request = api_broadcast_valid();
  handle_broadcast(con2->id, 123, con2->cc.pluginkeystring, request, &error);
  assert_false(error.isset);
  api_free_array(request);

  request = api_subscribe_valid();
  handle_unsubscribe(con1->id, 123, con1->cc.pluginkeystring, request, &error);
  assert_false(error.isset);
  api_free_array(request);

  request = api_subscribe_valid();
  handle_unsubscribe(con2->id, 123, con2->cc.pluginkeystring, request, &error);
  assert_false(error.isset);
  api_free_array(request);

  con1->closed = true;
  con2->closed = true;

  hashmap_free(cstr_t, ptr_t)(con1->subscribed_events);
  hashmap_free(cstr_t, ptr_t)(con2->subscribed_events);

  helper_free_plugin(plugin);
  connection_teardown();
  FREE(con1);
  FREE(con2);
  db_close();
}
