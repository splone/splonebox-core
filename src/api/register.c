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

int api_register(string api_key, string name, string desc, string author,
                 string license, struct message_params_object functions,
                 struct api_error *api_error)
{
  struct message_object *func;

  if (db_apikey_verify(api_key) == -1) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "API key is invalid.");
    return (-1);
  }

  if (functions.size == 0) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Error in register API request. Invalid params size.");
    return (-1);
  }

  if (db_plugin_add(api_key, name, desc, author, license) == -1) {
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

    if (db_function_add(api_key, &func->data.params) == -1) {
      error_set(api_error, API_ERROR_TYPE_VALIDATION,
          "Failed to register function in database.");
      continue;
    }
  }

  return (0);
}
