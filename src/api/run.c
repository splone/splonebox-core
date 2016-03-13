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
#include <stddef.h>

#include "api/sb-api.h"
#include "sb-common.h"

int api_run(string pluginlongtermpk, string function_name, uint64_t callid,
    struct message_object args, struct connection *con,
    uint32_t msgid, struct api_error *api_error)
{
  struct message_object *data;
  struct message_object *meta;
  array run_params;
  array run_response_params;
  string run;
  struct callinfo *cinfo;

  if (!api_error)
    return (-1);

  /* check if id is in database */
  if (db_apikey_verify(pluginlongtermpk) == -1) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "API key is invalid.");
    return (-1);
  }

  if (db_function_verify(pluginlongtermpk, function_name, &args.data.params) == -1) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "run() verification failed.");
    return (-1);
  }

  run_params.size = 3;
  run_params.obj = CALLOC(3, struct message_object);

  if (!run_params.obj)
    return (-1);

  /* data refs to first run_params parameter */
  meta = &run_params.obj[0];

  meta->type = OBJECT_TYPE_ARRAY;

  /* meta = [nil, callid] */
  meta->data.params.size = 2;
  meta->data.params.obj = CALLOC(2, struct message_object);

  if (!meta->data.params.obj)
    return (-1);

  /* add nil */
  data = &meta->data.params.obj[0];
  data->type = OBJECT_TYPE_NIL;

  /* add callid */
  data = &meta->data.params.obj[1];
  data->type = OBJECT_TYPE_UINT;
  data->data.uinteger = callid;

  /* add function name, data refs to second run_params parameter */
  data = &run_params.obj[1];
  data->type = OBJECT_TYPE_STR;
  data->data.string = cstring_copy_string(function_name.str);

  /* add function parameters, data refs to third run_params parameter */
  data = &run_params.obj[2];

  data->type = OBJECT_TYPE_ARRAY;
  data->data.params = message_object_copy(args).data.params;

  /* send request */
  run = (string) {.str = "run", .length = sizeof("run") - 1,};
  cinfo = connection_send_request(pluginlongtermpk, run, run_params,
      api_error);

  if (cinfo == NULL) {
      error_set(api_error, API_ERROR_TYPE_VALIDATION,
            "Error sending run API request.");
      return (-1);
  }

  if (cinfo->response.params.size != 1) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API response. Either response is broken "
        "or it just has wrong params size.");
    return (-1);
  }

  if (!(cinfo->response.params.obj[0].type == OBJECT_TYPE_UINT &&
    callid == cinfo->response.params.obj[0].data.uinteger)) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API response. Invalid callid");
    return (-1);
  }

  run_response_params.size = 1;
  run_response_params.obj = CALLOC(1, struct message_object);
  run_response_params.obj[0].type = OBJECT_TYPE_UINT;
  run_response_params.obj[0].data.uinteger = callid;

  if (connection_send_response(con, msgid, run_response_params, api_error) < 0) {
    return (-1);
  };

  free_params(cinfo->response.params);
  FREE(cinfo);

  return (0);
}
