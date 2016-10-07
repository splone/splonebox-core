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

#include "rpc/db/sb-db.h"
#include "api/sb-api.h"

int api_register(string name, string desc,
    string author, string license, array functions, struct connection *con,
    uint32_t msgid, struct api_error *api_error)
{
  struct message_object *func;
  array params = ARRAY_INIT;

  if (functions.size == 0) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error in register API request. Invalid params size.");
    return (-1);
  }

  if (db_plugin_add(con->cc.pluginkeystring, name, desc, author, license) == -1) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Failed to register plugin in database.");
    return (-1);
  }

  for (size_t i = 0; i < functions.size; i++) {
    func = &functions.obj[i];

    if (func->type != OBJECT_TYPE_ARRAY) {
      error_set(api_error, API_ERROR_TYPE_VALIDATION,
          "Function params has not expected type.");
      continue;
    }

    if (db_function_add(con->cc.pluginkeystring, &func->data.params) == -1) {
      error_set(api_error, API_ERROR_TYPE_VALIDATION,
          "Failed to register function in database.");
      continue;
    }
  }

  if (api_error->isset)
    return (-1);

  if (connection_send_response(con, msgid, params, api_error) < 0) {
    return (-1);
  };

  return (0);
}
