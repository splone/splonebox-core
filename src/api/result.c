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

int api_result(char *targetpluginkey, uint64_t callid,
    struct message_object args, struct connection *con, uint32_t msgid,
    struct api_error *api_error)
{
  struct message_object *data;
  struct message_object *meta;
  array result_params;
  array result_response_params;
  string result;
  struct callinfo cinfo;

  if (!api_error)
    return (-1);

  result_params.size = 2;
  result_params.obj = CALLOC(2, struct message_object);

  if (!result_params.obj)
    return (-1);

  /* data refs to first result_params parameter */
  meta = &result_params.obj[0];

  meta->type = OBJECT_TYPE_ARRAY;

  /* meta = [nil, callid] */
  meta->data.params.size = 1;
  meta->data.params.obj = CALLOC(1, struct message_object);

  if (!meta->data.params.obj)
    return (-1);

  /* add callid */
  data = &meta->data.params.obj[0];
  data->type = OBJECT_TYPE_UINT;
  data->data.uinteger = callid;

  /* add function parameters, data refs to third result_params parameter */
  data = &result_params.obj[1];

  data->type = OBJECT_TYPE_ARRAY;
  data->data.params = message_object_copy(args).data.params;

  /* send request */
  result = (string) {.str = "result", .length = sizeof("result") - 1};
  cinfo = connection_send_request(targetpluginkey, result, result_params,
      api_error);

  if (cinfo.response.params.size != 1) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching result API response. Either response is broken "
        "or it just has wrong params size.");
    return (-1);
  }

  if (!(cinfo.response.params.obj[0].type == OBJECT_TYPE_UINT &&
    callid == cinfo.response.params.obj[0].data.uinteger)) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API response. Invalid callid");
    return (-1);
  }

  result_response_params.size = 1;
  result_response_params.obj = CALLOC(1, struct message_object);
  result_response_params.obj[0].type = OBJECT_TYPE_UINT;
  result_response_params.obj[0].data.uinteger = callid;

  if (connection_send_response(con, msgid, result_response_params, api_error) < 0) {
    return (-1);
  };

  free_params(cinfo.response.params);

  return (0);
}
