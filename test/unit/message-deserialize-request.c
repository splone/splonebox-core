/**
 *    Copyright (C) 2016 splone UG
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


void unit_message_deserialize_request(UNUSED(void **state))
{
  struct api_error error = ERROR_INIT;
  msgpack_sbuffer sbuf;
  msgpack_packer pk;
  msgpack_zone mempool;
  msgpack_object deserialized;
  struct message_request request;

  msgpack_sbuffer_init(&sbuf);
  msgpack_zone_init(&mempool, 2048);

  /* positiv test */
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_array(&pk, 4);
  msgpack_pack_uint8(&pk, 0);
  msgpack_pack_uint32(&pk, 1234);
  msgpack_pack_str(&pk, 4);
  msgpack_pack_str_body(&pk, "test", 4);
  msgpack_pack_array(&pk, 1);
  msgpack_pack_uint8(&pk, 0);
  msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);

  assert_int_equal(0, message_deserialize_request(&request, &deserialized,
      &error));

  msgpack_sbuffer_clear(&sbuf);

  free_string(request.method);
  free_params(request.params);

  /* wrong type */
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_array(&pk, 4);
  msgpack_pack_uint8(&pk, 1);
  msgpack_pack_uint32(&pk, 1234);
  msgpack_pack_str(&pk, 4);
  msgpack_pack_str_body(&pk, "test", 4);
  msgpack_pack_array(&pk, 1);
  msgpack_pack_uint8(&pk, 0);
  msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);

  assert_int_not_equal(0, message_deserialize_request(&request, &deserialized,
      &error));

  msgpack_sbuffer_clear(&sbuf);

  /* wrong msgid */
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_array(&pk, 4);
  msgpack_pack_uint8(&pk, 0);
  msgpack_pack_int(&pk, -1234);
  msgpack_pack_str(&pk, 4);
  msgpack_pack_str_body(&pk, "test", 4);
  msgpack_pack_array(&pk, 1);
  msgpack_pack_uint8(&pk, 0);
  msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);

  assert_int_not_equal(0, message_deserialize_request(&request, &deserialized,
      &error));

  msgpack_sbuffer_clear(&sbuf);

  /* wrong method */
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_array(&pk, 4);
  msgpack_pack_uint8(&pk, 0);
  msgpack_pack_int(&pk, 1234);
  msgpack_pack_nil(&pk);
  msgpack_pack_array(&pk, 1);
  msgpack_pack_uint8(&pk, 0);
  msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);

  assert_int_not_equal(0, message_deserialize_request(&request, &deserialized,
      &error));

  msgpack_sbuffer_clear(&sbuf);

  /* wrong params */
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_array(&pk, 4);
  msgpack_pack_uint8(&pk, 0);
  msgpack_pack_int(&pk, 1234);
  msgpack_pack_str(&pk, 4);
  msgpack_pack_str_body(&pk, "test", 4);
  msgpack_pack_nil(&pk);
  msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);

  assert_int_not_equal(0, message_deserialize_request(&request, &deserialized,
      &error));

  msgpack_sbuffer_clear(&sbuf);

  msgpack_zone_destroy(&mempool);
  msgpack_sbuffer_destroy(&sbuf);
}
