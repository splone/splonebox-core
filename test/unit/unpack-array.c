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
#include "rpc/msgpack/sb-msgpack-rpc.h"
#include "helper-unix.h"


static msgpack_object deserialized;

void init_unpack_array(msgpack_object_type type)
{
  size_t off = 0;

  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);
  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  msgpack_pack_array(&pk, 3);
  if (type == MSGPACK_OBJECT_NIL) {
    msgpack_pack_nil(&pk);
  } else if (type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
    msgpack_pack_int(&pk, 1);
  } else if (type == MSGPACK_OBJECT_BOOLEAN) {
    msgpack_pack_true(&pk);
  } else if (type == MSGPACK_OBJECT_MAP) {
    msgpack_pack_map(&pk, 1);
  } else if (type == MSGPACK_OBJECT_ARRAY) {
    msgpack_pack_array(&pk, 3);
    msgpack_pack_int(&pk, 1);
    msgpack_pack_int(&pk, 1);
    msgpack_pack_int(&pk, 1);
  } else if (type == MSGPACK_OBJECT_FLOAT) {
    msgpack_pack_double(&pk, 1.2);
  }
  msgpack_pack_true(&pk);
  msgpack_pack_str(&pk, 7);
  msgpack_pack_str_body(&pk, "example", 7);

  msgpack_unpacked result;

  msgpack_unpacked_init(&result);
  msgpack_unpack_next(&result, sbuf.data, sbuf.size, &off);
  msgpack_sbuffer_destroy(&sbuf);
  deserialized = result.data;
}

void unit_unpack_array(UNUSED(void **state))
{
  struct message_params_object params;

  init_unpack_array(MSGPACK_OBJECT_NIL);
  assert_int_equal(0, unpack_params(&deserialized, &params));
  init_unpack_array(MSGPACK_OBJECT_ARRAY);
  assert_int_equal(0, unpack_params(&deserialized, &params));
  init_unpack_array(MSGPACK_OBJECT_FLOAT);
  assert_int_equal(0, unpack_params(&deserialized, &params));
  init_unpack_array(MSGPACK_OBJECT_MAP);
  assert_int_equal(0, unpack_params(&deserialized, &params));
  init_unpack_array(MSGPACK_OBJECT_POSITIVE_INTEGER);
  assert_int_equal(0, unpack_params(&deserialized, &params));
}
