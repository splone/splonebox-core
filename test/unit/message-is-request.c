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


void unit_message_is_request(UNUSED(void **state))
{
  struct api_error error = ERROR_INIT;
  msgpack_sbuffer sbuf;
  msgpack_packer pk;
  msgpack_zone mempool;
  msgpack_object deserialized;

  msgpack_sbuffer_init(&sbuf);
  msgpack_zone_init(&mempool, 2048);

  /* positiv test */
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_array(&pk, 4);
  msgpack_pack_uint8(&pk, 0);
  msgpack_pack_uint8(&pk, 0);
  msgpack_pack_uint8(&pk, 0);
  msgpack_pack_uint8(&pk, 0);
  msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);

  assert_true(message_is_request(&deserialized));

  msgpack_sbuffer_clear(&sbuf);

  /* wrong type test */
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_array(&pk, 4);
  msgpack_pack_uint8(&pk, 1);
  msgpack_pack_uint8(&pk, 1);
  msgpack_pack_uint8(&pk, 1);
  msgpack_pack_uint8(&pk, 1);
  msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);

  assert_false(message_is_request(&deserialized));

  msgpack_sbuffer_clear(&sbuf);

  /* no array test */
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_uint8(&pk, 1);
  msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);

  assert_false(message_is_request(&deserialized));

  msgpack_sbuffer_clear(&sbuf);

  /* NULL test */
  assert_false(message_is_request(NULL));

  msgpack_sbuffer_clear(&sbuf);

  msgpack_zone_destroy(&mempool);
  msgpack_sbuffer_destroy(&sbuf);
}
