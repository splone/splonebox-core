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


void unit_message_serialize_error_response(UNUSED(void **state))
{
  msgpack_sbuffer sbuf;
  struct api_error error = ERROR_INIT;
  msgpack_packer pk;

  error_set(&error, API_ERROR_TYPE_VALIDATION, "test error");

  msgpack_sbuffer_init(&sbuf);

  /* positiv test */
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  assert_int_equal(0, message_serialize_error_response(&pk, &error, 1234));
  msgpack_sbuffer_clear(&sbuf);

  /* null check */
  assert_int_not_equal(0, message_serialize_error_response(NULL, NULL, 1234));

  msgpack_sbuffer_destroy(&sbuf);
}
