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


void unit_unpack_string(UNUSED(void **state))
{
  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);
  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  msgpack_pack_str(&pk, 64);
  msgpack_pack_str_body(
      &pk,
      "TeiwieDoowuiMeix6SooxieFievee2io3ohhu5uo5ughu8cieja4iu6chuirijae",
      64);

  msgpack_object deserialized;
  msgpack_unpack(sbuf.data, sbuf.size, NULL, NULL, &deserialized);
  msgpack_sbuffer_destroy(&sbuf);

  assert_non_null(unpack_string(&deserialized).str);
}
