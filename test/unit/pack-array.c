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


void unit_pack_array(UNUSED(void **state))
{
  array params;
  msgpack_sbuffer sbuf;
  msgpack_packer pk;

  params.size = 4;
  params.obj =  CALLOC(params.size, struct message_object);

  params.obj[0].type = OBJECT_TYPE_UINT;
  params.obj[0].data.uinteger = 5;

  params.obj[1].type = OBJECT_TYPE_INT;
  params.obj[1].data.uinteger = 5;

  params.obj[2].type = OBJECT_TYPE_ARRAY;
  params.obj[2].data.params.size = 1;
  params.obj[2].data.params.obj = CALLOC(params.obj[2].data.params.size,
                                        struct message_object);
  params.obj[2].data.params.obj[0].type = OBJECT_TYPE_INT;
  params.obj[2].data.params.obj[0].data.uinteger = 6;

  msgpack_sbuffer_init(&sbuf);
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  assert_int_equal(0, pack_params(&pk, params));
}
