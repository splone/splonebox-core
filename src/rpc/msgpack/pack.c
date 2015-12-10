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

#include <stdbool.h>
#include <msgpack/pack.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "rpc/msgpack/sb-msgpack-rpc.h"
#include "sb-common.h"


int pack_string(msgpack_packer *pk, string str)
{
  if (!pk)
    return (-1);

  msgpack_pack_bin(pk, str.length);
  msgpack_pack_bin_body(pk, str.str, str.length);

  return (0);
}


int pack_uint8(msgpack_packer *pk, uint8_t uinteger)
{
  if (!pk)
    return (-1);

  msgpack_pack_uint8(pk, uinteger);

  return (0);
}


int pack_uint16(msgpack_packer *pk, uint16_t uinteger)
{
  if (!pk)
    return (-1);

  msgpack_pack_uint16(pk, uinteger);

  return (0);
}


int pack_uint32(msgpack_packer *pk, uint32_t uinteger)
{
  if (!pk)
    return (-1);

  msgpack_pack_uint32(pk, uinteger);

  return (0);
}


int pack_uint64(msgpack_packer *pk, uint64_t uinteger)
{
  if (!pk)
    return (-1);

  msgpack_pack_uint64(pk, uinteger);

  return (0);
}


int pack_int8(msgpack_packer *pk, int8_t integer)
{
  if (!pk)
    return (-1);

  msgpack_pack_int8(pk, integer);

  return (0);
}


int pack_int16(msgpack_packer *pk, int16_t integer)
{
  if (!pk)
    return (-1);

  msgpack_pack_int16(pk, integer);

  return (0);
}


int pack_int32(msgpack_packer *pk, int32_t integer)
{
  if (!pk)
    return (-1);

  msgpack_pack_int32(pk, integer);

  return (0);
}


int pack_int64(msgpack_packer *pk, int64_t integer)
{
  if (!pk)
    return (-1);

  msgpack_pack_int64(pk, integer);

  return (0);
}


int pack_bool(msgpack_packer *pk, bool boolean)
{
  if (!pk)
    return (-1);

  if (boolean)
    msgpack_pack_true(pk);
  else
    msgpack_pack_false(pk);

  return (0);
}


int pack_float(msgpack_packer *pk, double floating)
{
  if (!pk)
    return (-1);

  msgpack_pack_double(pk, floating);

  return (0);
}


int pack_nil(msgpack_packer *pk)
{
  if (!pk)
    return (-1);

  msgpack_pack_nil(pk);

  return (0);
}


int pack_params(msgpack_packer *pk, struct message_params_object params)
{
  size_t i;

  if (!pk)
    return (-1);

  msgpack_pack_array(pk, params.size);

  for (i = 0; i < params.size; i++) {
    message_object *object = &params.obj[i];
    message_object_type type = object->type;

    switch (type) {
    case (OBJECT_TYPE_INT):
      pack_int64(pk, object->data.integer);
      continue;
    case (OBJECT_TYPE_UINT):
      pack_uint64(pk, object->data.uinteger);
      continue;
    case (OBJECT_TYPE_BOOL):
      pack_bool(pk, object->data.boolean);
      continue;
    case (OBJECT_TYPE_FLOAT):
      pack_float(pk, object->data.floating);
      continue;
    case (OBJECT_TYPE_ARRAY):
      pack_params(pk, object->data.params);
      continue;
    case (OBJECT_TYPE_STR):
      /*  FALLTHROUGH */
    case (OBJECT_TYPE_BIN):
      pack_string(pk, object->data.string);
      continue;
    default:
      return (-1);
    }
  }

  return (0);
}
