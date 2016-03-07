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

#include <msgpack/object.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "rpc/msgpack/sb-msgpack-rpc.h"
#include "sb-common.h"

string unpack_string(msgpack_object *obj)
{
  string str;
  char *ret = MALLOC_ARRAY(obj->via.bin.size + 1, char);

  if (!ret || !obj->via.bin.ptr) {
    str = (string) {
      .str = NULL, .length = 0
    };
    return (str);
  }

  ret[obj->via.bin.size] = '\0';
  memcpy(ret, obj->via.bin.ptr, obj->via.bin.size);

  str = (string) {
    .str = ret, .length = obj->via.bin.size
  };

  return (str);
}


int64_t unpack_int(msgpack_object *obj)
{
  return (obj->via.i64);
}


uint64_t unpack_uint(msgpack_object *obj)
{
  return (obj->via.u64);
}


bool unpack_boolean(msgpack_object *obj)
{
  return (obj->via.boolean);
}


double unpack_float(msgpack_object *obj)
{
  return (obj->via.f64);
}


int unpack_params(msgpack_object *obj, array *params)
{
  struct message_object *elem;
  msgpack_object *tmp;

  if (!params)
    return (-1);

  /* if array is empty return success */
  if (obj->via.array.size <= 0) {
    params->obj = NULL;
    params->size = 0;
    return (0);
  }

  params->obj = CALLOC(obj->via.array.size, struct message_object);
  params->size = obj->via.array.size;
  
  if (!params->obj)
    return (-1);

  for (size_t i = 0; i < params->size; i++) {
    tmp = &obj->via.array.ptr[i];
    elem = &params->obj[i];

    switch (tmp->type) {
    case MSGPACK_OBJECT_POSITIVE_INTEGER:
      elem->type = OBJECT_TYPE_UINT;
      elem->data.uinteger = unpack_uint(tmp);
      continue;
    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
      elem->type = OBJECT_TYPE_INT;
      elem->data.integer = unpack_int(tmp);
      continue;
    case MSGPACK_OBJECT_STR:
      /* FALLTHROUGH */
    case MSGPACK_OBJECT_BIN:
      elem->type = OBJECT_TYPE_STR;
      elem->data.string = unpack_string(tmp);
      continue;
    case MSGPACK_OBJECT_BOOLEAN:
      elem->type = OBJECT_TYPE_BOOL;
      elem->data.boolean = unpack_boolean(tmp);
      continue;
    case MSGPACK_OBJECT_NIL:
      elem->type = OBJECT_TYPE_NIL;
      continue;
    case MSGPACK_OBJECT_FLOAT:
      elem->type = OBJECT_TYPE_FLOAT;
      elem->data.floating = unpack_float(tmp);
      continue;
    case MSGPACK_OBJECT_ARRAY:
      elem->type = OBJECT_TYPE_ARRAY;
      if (unpack_params(tmp, &elem->data.params) == -1)
        return (-1);
      continue;
    case MSGPACK_OBJECT_MAP:
      /* FALLTHROUGH */
    case MSGPACK_OBJECT_EXT:
      return (-1);
    default:
      return (-1);
    }
  }

  return (0);
}
