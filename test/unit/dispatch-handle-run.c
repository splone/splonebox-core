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
#include "helper-unix.h"


void unit_dispatch_handle_run(UNUSED(void **state))
{
  struct message_params_object *meta, *arguments;
  struct api_error *err = MALLOC(struct api_error);
  struct message_request *request = MALLOC(struct message_request);

  string apikey = cstring_copy_string(
      "vBXBg3Wkq3ESULkYWtijxfS5UvBpWb-2mZHpKAKpyRuTmvdy4WR7cTJqz-vi2BA2");
  string function_name = cstring_copy_string("test_function");
  string arg1 = cstring_copy_string("test arg1");
  string arg2 = cstring_copy_string("test arg2");

  /* first level arrays:
   *
   * [meta] args->obj[0]
   * [functions] args->obj[1]
   */
  request->params.size = 3;
  request->params.obj = CALLOC(3, struct message_object);
  request->params.obj[0].type = OBJECT_TYPE_ARRAY;
  request->params.obj[1].type = OBJECT_TYPE_STR;
  request->params.obj[1].data.string = function_name;
  request->params.obj[2].type = OBJECT_TYPE_ARRAY;

  /* meta array:
   *
   * [apikey]
   */
  meta = &request->params.obj[0].data.params;
  meta->size = 1;
  meta->obj = CALLOC(1, struct message_object);
  meta->obj[0].type = OBJECT_TYPE_STR;
  meta->obj[0].data.string = apikey;

  arguments = &request->params.obj[2].data.params;
  arguments->size = 2;
  arguments->obj = CALLOC(2, struct message_object);
  arguments->obj[0].type = OBJECT_TYPE_STR;
  arguments->obj[0].data.string = arg1;
  arguments->obj[1].type = OBJECT_TYPE_STR;
  arguments->obj[1].data.string = arg2;

  /* check NULL pointer error */
  assert_int_not_equal(0, handle_run(request, NULL, NULL));

  /* object has wrong type */
  request->params.obj[0].type = OBJECT_TYPE_STR;
  assert_int_not_equal(0, handle_run(request, NULL, err));

  /* reset */
  request->params.obj[0].type = OBJECT_TYPE_ARRAY;

  /* meta has wrong size */
  meta->size = 3;

  assert_int_not_equal(0, handle_run(request, NULL, err));

  /* reset */
  meta->size = 1;

  /* meta[0] has wrong type */
  meta->obj[0].type = OBJECT_TYPE_BIN;
  assert_int_not_equal(0, handle_run(request, NULL, err));

  free_params(request->params);
  FREE(request);
  FREE(err);
}
