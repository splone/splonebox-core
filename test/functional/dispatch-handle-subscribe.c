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
#include "rpc/connection/connection.h"
#include "api/helpers.h"
#include "api/sb-api.h"
#ifdef __linux__
#include <bsd/string.h>
#endif

#include "helper-unix.h"
#include "helper-all.h"
#include "helper-validate.h"

static array api_subscribe_valid(void)
{
  array request = ARRAY_DICT_INIT;
  ADD(request, STRING_OBJ(cstring_copy_string("testname")));

  return request;
}

static array api_subscribe_wrong_args_size(void)
{
  array request = ARRAY_DICT_INIT;
  ADD(request, STRING_OBJ(cstring_copy_string("testname")));
  ADD(request, STRING_OBJ(cstring_copy_string("wrong")));

  return request;
}

static array api_subscribe_wrong_args_type(void)
{
  array request = ARRAY_DICT_INIT;
  ADD(request, OBJECT_OBJ((object) OBJECT_INIT));

  return request;
}

void functional_dispatch_handle_subscribe(UNUSED(void **state))
{
  struct plugin *plugin = helper_get_example_plugin();
  struct connection *con1;
  struct connection *con2;
  struct api_error error = ERROR_INIT;
  array request;

  con1 = calloc_or_die(1, sizeof(struct connection));
  con1->id = (uint64_t) randommod(281474976710656LL);
  con1->msgid = 4321;
  con1->subscribed_events = hashmap_new(cstr_t, ptr_t)();

  con2 = calloc_or_die(1, sizeof(struct connection));
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



  //expect_check(__wrap_crypto_write, &deserialized, validate_run_request, plugin);

  request = api_subscribe_valid();
  handle_subscribe(con1->id, 123, con1->cc.pluginkeystring, request, &error);
  assert_false(error.isset);
  api_free_array(request);

  request = api_subscribe_valid();
  handle_subscribe(con2->id, 123, con2->cc.pluginkeystring, request, &error);
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

  request = api_subscribe_wrong_args_size();
  handle_subscribe(con1->id, 123, con1->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  error.isset = false;
  api_free_array(request);

  request = api_subscribe_wrong_args_type();
  handle_subscribe(con1->id, 123, con1->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  error.isset = false;
  api_free_array(request);

  request = api_subscribe_wrong_args_size();
  handle_unsubscribe(con1->id, 123, con1->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  error.isset = false;
  api_free_array(request);

  request = api_subscribe_wrong_args_type();
  handle_unsubscribe(con1->id, 123, con1->cc.pluginkeystring, request, &error);
  assert_true(error.isset);
  error.isset = false;
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
