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
 *
 *    This file incorporates code covered by the following terms:
 *
 *    Copyright Neovim contributors. All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

 #include <stdlib.h>
 #include <stddef.h>

 #include "api/sb-api.h"
 #include "api/helpers.h"
 #include "sb-common.h"

void api_free_string(string value)
{
  if (!value.str) {
    return;
  }

  FREE(value.str);
}

void api_free_object(object value)
{
  switch (value.type) {
    case OBJECT_TYPE_NIL:
    case OBJECT_TYPE_BOOL:
    case OBJECT_TYPE_INT:
    case OBJECT_TYPE_UINT:
    case OBJECT_TYPE_FLOAT:
      break;

    case OBJECT_TYPE_STR:
      api_free_string(value.data.string);
      break;

    case OBJECT_TYPE_ARRAY:
      api_free_array(value.data.array);
      break;

    case OBJECT_TYPE_DICTIONARY:
      api_free_dictionary(value.data.dictionary);
      break;

    default:
      abort();
  }
}

void api_free_array(array value)
{
  for (size_t i = 0; i < value.size; i++) {
    api_free_object(value.items[i]);
  }

  FREE(value.items);
}

void api_free_dictionary(dictionary value)
{
  for (size_t i = 0; i < value.size; i++) {
    api_free_string(value.items[i].key);
    api_free_object(value.items[i].value);
  }

  FREE(value.items);
}

object copy_object(object obj)
{
  switch (obj.type) {
    case OBJECT_TYPE_NIL:
    case OBJECT_TYPE_BOOL:
    case OBJECT_TYPE_INT:
    case OBJECT_TYPE_UINT:
    case OBJECT_TYPE_FLOAT:
      return obj;

    case OBJECT_TYPE_STR:
      return STRING_OBJ(cstring_copy_string(obj.data.string.str));

    case OBJECT_TYPE_ARRAY: {
      array rv = ARRAY_DICT_INIT;
      for (size_t i = 0; i < obj.data.array.size; i++) {
        ADD(rv, copy_object(obj.data.array.items[i]));
      }
      return ARRAY_OBJ(rv);
    }

    case OBJECT_TYPE_DICTIONARY: {
      dictionary rv = ARRAY_DICT_INIT;
      for (size_t i = 0; i < obj.data.dictionary.size; i++) {
        key_value_pair item = obj.data.dictionary.items[i];
        PUT(rv, item.key.str, copy_object(item.value));
      }
      return DICTIONARY_OBJ(rv);
    }
    default:
      abort();
  }
}
