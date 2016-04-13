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


void unit_message_serialize_response(UNUSED(void **state))
{
  msgpack_sbuffer sbuf;
  msgpack_packer pk;
  struct message_response response;
  array params;

  params.size = 1;
  params.obj = CALLOC(1, struct message_object);
  params.obj[0].type = OBJECT_TYPE_UINT;
  params.obj[0].data.uinteger = 1234;

  msgpack_sbuffer_init(&sbuf);

  /* positiv test */
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  response.msgid = 1234;
  response.params = params;
  assert_int_equal(0, message_serialize_response(&response, &pk));
  msgpack_sbuffer_clear(&sbuf);

  free_params(response.params);

  params.size = 1;
  params.obj = CALLOC(1, struct message_object);
  params.obj[0].type = 1000;
  params.obj[0].data.uinteger = 1234;

  /* no valid params */
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  response.msgid = 1234;
  response.params = params;
  assert_int_not_equal(0, message_serialize_response(&response, &pk));
  msgpack_sbuffer_clear(&sbuf);

  free_params(response.params);

  /* null check */
  assert_int_not_equal(0, message_serialize_response(NULL, NULL));

  msgpack_sbuffer_destroy(&sbuf);
}
