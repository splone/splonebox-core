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

void unit_dispatch_handle_register(UNUSED(void **state))
{
  struct message_request *request;
  struct message_params_object *meta;
  struct api_error *err = MALLOC(struct api_error);

  string apikey = cstring_copy_string(
      "vBXBg3Wkq3ESULkYWtijxfS5UvBpWb-2mZHpKAKpyRuTmvdy4WR7cTJqz-vi2BA2");
  string name = cstring_copy_string("register");
  string description = cstring_copy_string("register a plugin");
  string author = cstring_copy_string("test");
  string license = cstring_copy_string("none");

  request = MALLOC(struct message_request);


  /* first level arrays:
   *
   * [meta] args->obj[0]
   * [functions] args->obj[1]
   */
  request->params.size = 2;
  request->params.obj = CALLOC(2, struct message_object);
  request->params.obj[0].type = OBJECT_TYPE_ARRAY;
  request->params.obj[1].type = OBJECT_TYPE_ARRAY;

  /* meta array:
   *
   * [name]
   * [desciption]
   * [author]
   * [license]
   */
  meta = &request->params.obj[0].data.params;
  meta->size = 5;
  meta->obj = CALLOC(5, struct message_object);
  meta->obj[0].type = OBJECT_TYPE_STR;
  meta->obj[0].data.string = apikey;
  meta->obj[1].type = OBJECT_TYPE_STR;
  meta->obj[1].data.string = name;
  meta->obj[2].type = OBJECT_TYPE_STR;
  meta->obj[2].data.string = description;
  meta->obj[3].type = OBJECT_TYPE_STR;
  meta->obj[3].data.string = author;
  meta->obj[4].type = OBJECT_TYPE_STR;
  meta->obj[4].data.string = license;

  /* check NULL pointer error */
  assert_int_not_equal(0, handle_register(request, NULL, NULL));

  /* object is correct */
  assert_int_equal(0, handle_register(request, NULL, err));

  /* object has wrong type */
  request->params.obj[0].type = OBJECT_TYPE_STR;
  assert_int_not_equal(0, handle_register(request, NULL, err));

  /* reset */
  request->params.obj[0].type = OBJECT_TYPE_ARRAY;

  /* meta has wrong size */
  meta->size = 3;
  assert_int_not_equal(0, handle_register(request, NULL, err));

  /* reset */
  meta->size = 4;

  /* meta[0] has wrong type */
  meta->obj[0].type = OBJECT_TYPE_BIN;
  assert_int_not_equal(0, handle_register(request, NULL, err));

  meta->size = 5;

  free_params(request->params);
  FREE(request);
  FREE(err);
}
