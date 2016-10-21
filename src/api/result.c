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
#include "api/helpers.h"
#include "sb-common.h"

int api_result(char *targetpluginkey, uint64_t callid, array args,
    struct api_error *api_error)
{
  string result;
  object res;

  if (!api_error)
    return (-1);

  array meta = ARRAY_DICT_INIT;
  ADD(meta, UINTEGER_OBJ(callid));

  array request = ARRAY_DICT_INIT;
  ADD(request, ARRAY_OBJ(meta));
  ADD(request, copy_object(ARRAY_OBJ(args)));

  /* send request */
  result = (string) {.str = "result", .length = sizeof("result") - 1};
  res = connection_send_request(targetpluginkey, result, request,
      api_error);

  if (api_error->isset)
    return (-1);

  if (res.data.array.size != 1) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching result API response. Either response is broken "
        "or it just has wrong params size.");
    return (-1);
  }

  if (!(res.data.array.items[0].type == OBJECT_TYPE_UINT &&
    callid == res.data.array.items[0].data.uinteger)) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error dispatching run API response. Invalid callid");
    return (-1);
  }

  api_free_array(res.data.array);

  return (0);
}
