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
 *
 *    This file incorporates code covered by the following terms:
 *
 *    Copyright Neovim contributors. All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <stdlib.h>

#include "rpc/sb-rpc.h"
#include "api/sb-api.h"
#include "api/helpers.h"
#include "sb-common.h"

static msgpack_sbuffer sbuf;
static hashmap(string, dispatch_info) *dispatch_table = NULL;
static hashmap(uint64_t, ptr_t) *callids = NULL;

object msgpack_rpc_handle_missing_method(UNUSED(uint64_t channel_id),
    UNUSED(uint64_t msgid), UNUSED(char *pluginkey), UNUSED(array args),
    struct api_error *error)
{
  snprintf(error->msg, sizeof(error->msg), "Invalid method name");
  error->isset = true;
  return NIL;
}

object msgpack_rpc_handle_invalid_arguments(UNUSED(uint64_t channel_id),
    UNUSED(uint64_t msgid), UNUSED(char *pluginkey), UNUSED(array args),
    struct api_error *error)
{
  snprintf(error->msg, sizeof(error->msg), "Invalid method arguments");
  error->isset = true;
  return NIL;
}

/*
 * Dispatch a register message to API-register function
 *
 * @param params register arguments saved in `array`
 * @param api_error `struct api_error` error object-instance
 * @return 0 if success, -1 otherwise
 */
object handle_register(UNUSED(uint64_t con_id), UNUSED(uint64_t msgid),
    char *pluginkey, array args, struct api_error *error)
{
  array rv = ARRAY_DICT_INIT;
  object ret = ARRAY_OBJ(rv);
  array meta = ARRAY_DICT_INIT;
  array functions = ARRAY_DICT_INIT;
  string name, description, author, license;

  if (!error)
    goto end;

  /* check params size */
  if (args.size != 2) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. Invalid params size");
    goto end;
  }

  if (args.items[0].type == OBJECT_TYPE_ARRAY)
    meta = args.items[0].data.array;
  else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. meta params has wrong type");
    goto end;
  }

  /*
   * meta params:
   * [name, description, author, license]
   */

  if (meta.size != 4) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. Invalid meta params size");
    goto end;
  }

  /* extract meta information */
  if ((meta.items[0].type != OBJECT_TYPE_STR) ||
      (meta.items[1].type != OBJECT_TYPE_STR) ||
      (meta.items[2].type != OBJECT_TYPE_STR) ||
      (meta.items[3].type != OBJECT_TYPE_STR)) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. meta element has wrong type");
    goto end;
  }

  if (!meta.items[0].data.string.str || !meta.items[1].data.string.str ||
      !meta.items[2].data.string.str || !meta.items[3].data.string.str) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. Invalid meta params size");
    goto end;
  }

  name = meta.items[0].data.string;
  description = meta.items[1].data.string;
  author = meta.items[2].data.string;
  license = meta.items[3].data.string;

  if (args.items[1].type == OBJECT_TYPE_ARRAY) {
    functions = args.items[1].data.array;
  } else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. functions has wrong type");
    goto end;
  }

  if (api_register(name, description, author, license, functions, pluginkey,
      error) == -1) {

    if (!error->isset)
      error_set(error, API_ERROR_TYPE_VALIDATION,
         "Error running register API request.");

    goto end;
  }

end:
  return ret;
}


object handle_run(UNUSED(uint64_t con_id), UNUSED(uint64_t msgid),
    char *pluginkey, array args, struct api_error *error)
{
  array rv = ARRAY_DICT_INIT;
  array meta = ARRAY_DICT_INIT;
  array runargs = ARRAY_DICT_INIT;
  string function_name = STRING_INIT;
  object ret = ARRAY_OBJ(rv);
  uint64_t callid;
  char *targetpluginkey;

  if (!error)
    goto end;

  /* check params size */
  if (args.size != 3) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. Invalid params size");
    goto end;
  }

  if (args.items[0].type == OBJECT_TYPE_ARRAY)
    meta = args.items[0].data.array;
  else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. meta params has wrong type");
    goto end;
  }

  /* meta = [targetpluginkey, nil]*/
  if (meta.size != 2) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. Invalid meta params size");
    goto end;
  }

  /* extract meta information */
  if (meta.items[0].type != OBJECT_TYPE_STR) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. meta elements have wrong type");
    goto end;
  }

  if (!meta.items[0].data.string.str ||
    meta.items[0].data.string.length + 1 != PLUGINKEY_STRING_SIZE) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. Invalid meta params size");
    goto end;
  }

  targetpluginkey = meta.items[0].data.string.str;
  to_upper(targetpluginkey);

  if (meta.items[1].type != OBJECT_TYPE_NIL) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. meta elements have wrong type");
    goto end;
  }

  if (args.items[1].type == OBJECT_TYPE_STR) {
    function_name = args.items[1].data.string;
  } else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. function string has wrong type");
    goto end;
  }

  if (args.items[2].type == OBJECT_TYPE_ARRAY) {
    runargs = args.items[2].data.array;
  } else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. function string has wrong type");
    goto end;
  }

  callid = (uint64_t) randommod(281474976710656LL);
  LOG_VERBOSE(VERBOSE_LEVEL_1, "generated callid %lu\n", callid);
  hashmap_put(uint64_t, ptr_t)(callids, callid, pluginkey);

  if (api_run(targetpluginkey, function_name, callid, runargs, error) == -1) {
    if (false == error->isset)
      error_set(error, API_ERROR_TYPE_VALIDATION,
         "Error executing run API request.");
    goto end;
  }

  ret = ARRAY_OBJ(((array) {
    .size = 1,
    .capacity = 1,
    .items = &UINTEGER_OBJ(callid)
  }));

end:
  return ret;
}

object handle_result(UNUSED(uint64_t con_id), UNUSED(uint64_t msgid),
    UNUSED(char *pluginkey), array args, struct api_error *error)
{
  array rv = ARRAY_DICT_INIT;
  array meta = ARRAY_DICT_INIT;
  array resultargs = ARRAY_DICT_INIT;
  object ret = ARRAY_OBJ(rv);
  uint64_t callid;

  char * targetpluginkey;

  if (!error)
    goto end;

  /* check params size */
  if (args.size != 2) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching result API request. Invalid params size");
    goto end;
  }

  if (args.items[0].type == OBJECT_TYPE_ARRAY)
    meta = args.items[0].data.array;
  else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching result API request. meta params has wrong type");
    goto end;
  }

  /* meta = [callid]*/
  if (meta.size != 1) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching result API request. Invalid meta params size");
    goto end;
  }

  /* extract meta information */
  if (meta.items[0].type == OBJECT_TYPE_UINT) {
    callid = meta.items[0].data.uinteger;
  } else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching result API request. meta elements have wrong type");
    goto end;
  }

  if (args.items[1].type == OBJECT_TYPE_ARRAY) {
    resultargs = args.items[1].data.array;
  } else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching result API request. function string has wrong type");
    goto end;
  }

  targetpluginkey = hashmap_get(uint64_t, ptr_t)(callids, callid);

  if (!targetpluginkey) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
      "Failed to find target's key associated with given callid.");
    goto end;
  }

  if (api_result(targetpluginkey, callid, resultargs, error) == -1) {
    if (false == error->isset)
      error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error executing result API request.");
    goto end;
  }

  hashmap_del(uint64_t, ptr_t)(callids, callid);

  ret = ARRAY_OBJ(((array) {
    .size = 1,
    .capacity = 1,
    .items = &UINTEGER_OBJ(callid)
  }));

end:
  return ret;
}


object handle_broadcast(UNUSED(uint64_t con_id), UNUSED(uint64_t msgid),
    UNUSED(char *pluginkey), array args, struct api_error *error)
{
  array rv = ARRAY_DICT_INIT;
  array eventargs = ARRAY_DICT_INIT;
  object ret = ARRAY_OBJ(rv);
  string eventname = STRING_INIT;

  if (!error)
    goto end;

  /* check params size */
  if (args.size != 2) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching broadcast API request. Invalid params size");
    goto end;
  }

  if (args.items[0].type == OBJECT_TYPE_STR)
    eventname = args.items[0].data.string;
  else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching broadcast API request. event name has wrong type");
    goto end;
  }

  if (args.items[1].type == OBJECT_TYPE_ARRAY)
    eventargs = args.items[1].data.array;
  else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching broadcast API request. event args has wrong type");
    goto end;
  }

  if (api_broadcast(eventname, eventargs, error) == -1) {
    if (false == error->isset)
      error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error executing broadcast API request.");
    goto end;
  }

end:
  return ret;
}


object handle_subscribe(uint64_t con_id, UNUSED(uint64_t msgid),
    UNUSED(char *pluginkey), array args, struct api_error *error)
{
  array rv = ARRAY_DICT_INIT;
  object ret = ARRAY_OBJ(rv);
  string eventname = STRING_INIT;

  if (!error)
    goto end;

  /* check params size */
  if (args.size != 1) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching broadcast API request. Invalid params size");
    goto end;
  }

  if (args.items[0].type == OBJECT_TYPE_STR)
    eventname = args.items[0].data.string;
  else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching broadcast API request. event name has wrong type");
    goto end;
  }

  if (api_subscribe(con_id, eventname, error) == -1) {
    if (false == error->isset)
      error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error executing broadcast API request.");
    goto end;
  }

end:
  return ret;
}

object handle_unsubscribe(uint64_t con_id, UNUSED(uint64_t msgid),
    UNUSED(char *pluginkey), array args, struct api_error *error)
{
  array rv = ARRAY_DICT_INIT;
  object ret = ARRAY_OBJ(rv);
  string eventname = STRING_INIT;

  if (!error)
    goto end;

  /* check params size */
  if (args.size != 1) {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching broadcast API request. Invalid params size");
    goto end;
  }

  if (args.items[0].type == OBJECT_TYPE_STR)
    eventname = args.items[0].data.string;
  else {
    error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching broadcast API request. event name has wrong type");
    goto end;
  }

  if (api_unsubscribe(con_id, eventname, error) == -1) {
    if (false == error->isset)
      error_set(error, API_ERROR_TYPE_VALIDATION,
        "Error executing broadcast API request.");
    goto end;
  }

end:
  return ret;
}


void dispatch_table_put(string method, dispatch_info info)
{
  hashmap_put(string, dispatch_info)(dispatch_table, method, info);
}


dispatch_info dispatch_table_get(string method)
{
  return (hashmap_get(string, dispatch_info)(dispatch_table, method));
}

int dispatch_teardown(void)
{
  hashmap_free(string, dispatch_info)(dispatch_table);

  hashmap_free(uint64_t, ptr_t)(callids);

  return (0);
}

dispatch_info msgpack_rpc_get_handler_for(const char *name, size_t name_len)
{
  string m = { .str = (char *)name, .length = name_len };
  dispatch_info rv =
    hashmap_get(string, dispatch_info)(dispatch_table, m);

  if (!rv.func) {
    rv.func = msgpack_rpc_handle_missing_method;
  }

  return rv;
}


int dispatch_table_init(void)
{
  dispatch_info register_info = {.func = handle_register, .async = true,
      .name = (string) {.str = "register", .length = sizeof("register") - 1}};
  dispatch_info run_info = {.func = handle_run, .async = true,
      .name = (string) {.str = "run", .length = sizeof("run") - 1}};
  dispatch_info result_info = {.func = handle_result, .async = true,
      .name = (string) {.str = "result", .length = sizeof("result") - 1,}};
  dispatch_info broadcast_info = {.func = handle_broadcast, .async = true,
      .name = (string) {.str = "broadcast", .length = sizeof("broadcast") - 1,}};
  dispatch_info subscribe_info = {.func = handle_subscribe, .async = false,
      .name = (string) {.str = "subscribe", .length = sizeof("subscribe") - 1,}};
  dispatch_info unsubscribe_info = {.func = handle_unsubscribe, .async = false,
      .name = (string) {.str = "unsubscribe", .length = sizeof("unsubscribe") - 1,}};


  msgpack_sbuffer_init(&sbuf);

  dispatch_table = hashmap_new(string, dispatch_info)();
  callids = hashmap_new(uint64_t, ptr_t)();

  if (!dispatch_table || !callids)
    return (-1);

  dispatch_table_put(register_info.name, register_info);
  dispatch_table_put(run_info.name, run_info);
  dispatch_table_put(result_info.name, result_info);
  dispatch_table_put(broadcast_info.name, broadcast_info);
  dispatch_table_put(subscribe_info.name, subscribe_info);
  dispatch_table_put(unsubscribe_info.name, unsubscribe_info);

  return (0);
}
