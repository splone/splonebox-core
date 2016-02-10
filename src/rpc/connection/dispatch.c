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

#include <stdlib.h>

#include "rpc/sb-rpc.h"
#include "api/sb-api.h"
#include "sb-common.h"

static msgpack_sbuffer sbuf;
static struct hashmap_string *connections = NULL;
static struct hashmap_string *dispatch_table = NULL;
static struct hashmap_uint64 *callids = NULL;

int handle_error(connection_request_event_info *info)
{
  struct message_request *request = info->request;
  struct api_error *api_error = &info->api_error;

  if (!request || !api_error )
    return (-1);

  return (0);
}

/*
 * Dispatch a register message to API-register function
 *
 * @param params register arguments saved in `struct message_params_object`
 * @param api_error `struct api_error` error object-instance
 * @return 0 if success, -1 otherwise
 */
int handle_register(connection_request_event_info *info)
{
  struct message_params_object *meta;
  struct message_params_object functions;
  string pluginlongtermpk, name, description, author, license;

  struct connection *con;
  char *data;
  msgpack_packer packer;

  struct message_request *request = info->request;
  struct api_error *api_error = &info->api_error;

  if (!api_error)
    return (-1);

  /* check params size */
  if (request->params.size != 2) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. Invalid params params size");
    return (-1);
  }

  if (request->params.obj[0].type == OBJECT_TYPE_ARRAY)
    meta = &request->params.obj[0].data.params;
  else {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. meta params has wrong type");
    return (-1);
  }

  /*
   * meta params:
   * [pluginlongtermpk, name, description, author, license]
   */

  if (!meta) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. meta params is NULL");
    return (-1);
  }

  if (meta->size != 5) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. Invalid meta params size");
    return (-1);
  }

  /* extract meta information */
  if ((meta->obj[0].type != OBJECT_TYPE_STR) ||
      (meta->obj[1].type != OBJECT_TYPE_STR) ||
      (meta->obj[2].type != OBJECT_TYPE_STR) ||
      (meta->obj[3].type != OBJECT_TYPE_STR) ||
      (meta->obj[4].type != OBJECT_TYPE_STR)) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. meta element has wrong type");
    return (-1);
  }

  if (!meta->obj[0].data.string.str || !meta->obj[1].data.string.str ||
      !meta->obj[2].data.string.str || !meta->obj[3].data.string.str ||
      !meta->obj[4].data.string.str) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. Invalid meta params size");
    return (-1);
  }

  pluginlongtermpk = meta->obj[0].data.string;
  name = meta->obj[1].data.string;
  description = meta->obj[2].data.string;
  author = meta->obj[3].data.string;
  license = meta->obj[4].data.string;

  if (request->params.obj[1].type != OBJECT_TYPE_ARRAY) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. functions has wrong type");
    return (-1);
  }

  functions = request->params.obj[1].data.params;

  if (api_register(pluginlongtermpk, name, description, author, license, functions,
      api_error)) {
    return (-1);
  }

  /* TODO: pack status response */

  /*
   * add connection with the client long term public key as key to the
   * connection hashmap
   */

  hashmap_string_put(connections, pluginlongtermpk, info->con);

  if (api_error->isset)
    message_serialize_error_response(&packer, api_error, request->msgid);

  data = MALLOC_ARRAY(sbuf.size, char);

  if (data == NULL)
    return (-1);

  outputstream_write(info->con->streams.write, memcpy(data, sbuf.data,
      sbuf.size), sbuf.size);

  msgpack_sbuffer_clear(&sbuf);

  return (0);
}


int handle_run(connection_request_event_info *info)
{
  struct message_params_object *meta;
  struct message_params_object args;
  string pluginlongtermpk, function_name;
  struct connection *con;
  char *data;
  msgpack_packer packer;

  struct message_request *request = info->request;
  struct api_error *api_error = &info->api_error;

  if (!api_error )
    return (-1);

  /* check params size */
  if (request->params.size != 3) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. Invalid params params size");
    return (-1);
  }

  if (request->params.obj[0].type == OBJECT_TYPE_ARRAY)
    meta = &request->params.obj[0].data.params;
  else {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. meta params has wrong type");
    return (-1);
  }

  if (!meta) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. meta params is NULL");
    return (-1);
  }

  if (meta->size != 1) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. Invalid meta params size");
    return (-1);
  }

  /* extract meta information */
  if (meta->obj[0].type != OBJECT_TYPE_STR) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. meta elements have wrong type");
    return (-1);
  }

  if (!meta->obj[0].data.string.str) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. Invalid meta params size");
    return (-1);
  }

  pluginlongtermpk = meta->obj[0].data.string;

  if (request->params.obj[1].type != OBJECT_TYPE_STR) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. function string has wrong type");
    return (-1);
  }

  if (!request->params.obj[1].data.string.str) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching register API request. Invalid meta params size");
    return (-1);
  }

  function_name = request->params.obj[1].data.string;

  if (request->params.obj[2].type != OBJECT_TYPE_ARRAY) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API request. function string has wrong type");
    return (-1);
  }

  args = request->params.obj[2].data.params;

  msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);

  api_run(pluginlongtermpk, function_name, args, &packer, api_error);

  con = hashmap_get(connections, pluginlongtermpk);

  if (api_error->isset)
    message_serialize_error_response(&packer, api_error, request->msgid);

  data = MALLOC_ARRAY(sbuf.size, char);

  if (data == NULL)
    return (-1);

  outputstream_write(con->streams.write, memcpy(data, sbuf.data, sbuf.size),
      sbuf.size);

  msgpack_sbuffer_clear(&sbuf);

  return (0);
}


void dispatch_table_put(string method, struct dispatch_info *info)
{
  hashmap_string_put(dispatch_table, method, info);
}


struct dispatch_info *dispatch_table_get(string method)
{
  struct dispatch_info *info;

  info = (struct dispatch_info *)hashmap_string_get(dispatch_table, method);

  if (!info)
    return (NULL);

  return (info);
}

int dispatch_table_free(void)
{
  struct dispatch_info *info;

  HASHMAP_ITERATE_VALUE(dispatch_table, info, {
    free_string(info->name);
    FREE(info);
  });

  hashmap_string_free(dispatch_table);

  return (0);
}


int dispatch_table_init(void)
{

  struct dispatch_info *register_info, *run_info;

  msgpack_sbuffer_init(&sbuf);

  connections = hashmap_string_new();
  dispatch_table = hashmap_string_new();
  callids = hashmap_uint64_new();

  if (!connections)
    return (-1);

  if (!dispatch_table)
    return (-1);

  /* register */
  register_info = MALLOC(struct dispatch_info);

  if (!register_info)
    return (-1);

  register_info->func = handle_register;
  register_info->async = true;
  register_info->name = cstring_copy_string("register");

  dispatch_table_put(register_info->name, register_info);

  /* run */
  run_info = MALLOC(struct dispatch_info);

  if (!run_info)
    return (-1);

  run_info->func = handle_run;
  run_info->async = false;
  run_info->name = cstring_copy_string("run");

  dispatch_table_put(run_info->name, run_info);

  return (0);
}
