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

#include "helper-unix.h"
#include "sb-common.h"
#include "api/helpers.h"
#include "rpc/msgpack/helpers.h"


void functional_msgpack_rpc_helper(UNUSED(void **state))
{
  msgpack_sbuffer sbuf;
  msgpack_packer pk;

  msgpack_sbuffer_init(&sbuf);
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  // declare test object with all available types
  array test = ARRAY_DICT_INIT;

  dictionary dict_test = ARRAY_DICT_INIT;

  dictionary dict_abc = ARRAY_DICT_INIT;
  PUT(dict_abc, "123", INTEGER_OBJ(123));
  PUT(dict_abc, "456", STRING_OBJ(cstring_copy_string("456")));

  dictionary dict_def = ARRAY_DICT_INIT;
  PUT(dict_def, "789", INTEGER_OBJ(789));
  PUT(dict_def, "987", STRING_OBJ(cstring_copy_string("987")));

  dictionary dict_ghi = ARRAY_DICT_INIT;
  PUT(dict_ghi, "654", INTEGER_OBJ(654));
  PUT(dict_ghi, "321", STRING_OBJ(cstring_copy_string("321")));

  PUT(dict_test, "abc", DICTIONARY_OBJ(dict_abc));
  PUT(dict_test, "def", DICTIONARY_OBJ(dict_def));
  PUT(dict_test, "ghi", DICTIONARY_OBJ(dict_ghi));

  array array_abc = ARRAY_DICT_INIT;
  ADD(array_abc, OBJECT_OBJ((object) OBJECT_INIT));
  ADD(array_abc, FLOATING_OBJ(1.2345));
  ADD(array_abc, UINTEGER_OBJ(1234));
  ADD(array_abc, INTEGER_OBJ(-1234));

  array array_def = ARRAY_DICT_INIT;
  ADD(array_def, STRING_OBJ(cstring_copy_string("test")));
  ADD(array_def, BOOLEAN_OBJ(true));
  ADD(array_def, BOOLEAN_OBJ(false));

  ADD(test, ARRAY_OBJ(array_abc));
  ADD(test, ARRAY_OBJ(array_def));
  ADD(test, DICTIONARY_OBJ(dict_test));

  object serialize_object = ARRAY_OBJ(test);

  msgpack_rpc_from_object(serialize_object, &pk);
  api_free_object(serialize_object);

  msgpack_zone mempool;
  msgpack_zone_init(&mempool, 2048);

  msgpack_object deserialized;
  msgpack_unpack(sbuf.data, sbuf.size, NULL, &mempool, &deserialized);

  array deserialized_array;
  assert_true(msgpack_rpc_to_array(&deserialized, &deserialized_array));
  api_free_array(deserialized_array);

  object deserialized_object;
  assert_true(msgpack_rpc_to_object(&deserialized, &deserialized_object));
  api_free_object(deserialized_object);

  //api_free_object(serialize_object);
  //api_free_array(deserialized_array);
  //api_free_object(deserialized_object);

  msgpack_zone_destroy(&mempool);
  msgpack_sbuffer_destroy(&sbuf);
}
