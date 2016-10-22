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
#include "api/helpers.h"


int api_broadcast(string event, array args, struct api_error *api_error)
{
  if (!api_error)
    return -1;

  object o = copy_object(ARRAY_OBJ(args));

  if (!connection_send_event(0, event.str, o.data.array)) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION,
        "Broadcasting event failed");
    return -1;
  }

  return 0;
}
