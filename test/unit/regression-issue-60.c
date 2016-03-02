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

void unit_regression_issue_60(UNUSED(void **state))
{
  struct message_params_object params;
  struct msgpack_object deserialized;
  msgpack_unpacked result;
  msgpack_sbuffer sbuf;
  msgpack_packer pk;

  size_t off = 0;

  msgpack_sbuffer_init(&sbuf);
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  msgpack_pack_array(&pk, 0);

  msgpack_unpacked_init(&result);
  msgpack_unpack_next(&result, sbuf.data, sbuf.size, &off);
  msgpack_sbuffer_destroy(&sbuf);

  deserialized = result.data;

  assert_int_equal(0, unpack_params(&deserialized, &params));
  free_params(params);
  msgpack_unpacked_destroy(&result);
}
