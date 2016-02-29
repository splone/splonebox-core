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

#include "sb-common.h"

struct hashmap_uint64 *hashmap_uint64_new()
{
  struct hashmap_uint64 *hmap = MALLOC(struct hashmap_uint64);

  if (hmap == NULL)
    return (NULL);

  hmap->table = kh_init(HASHMAP_UINT64);

  return (hmap);
}


void *hashmap_uint64_get(struct hashmap_uint64 *hmap, uint64_t key)
{
  khiter_t i;

  if ((i = kh_get(HASHMAP_UINT64, hmap->table, key)) == kh_end(hmap->table))
    return (NULL);

  return (kh_val(hmap->table, i));
}


bool hashmap_uint64_contains_key(struct hashmap_uint64 *hmap, uint64_t key)
{
  return (kh_get(HASHMAP_UINT64, hmap->table, key) != kh_end(hmap->table));
}


void *hashmap_uint64_put(struct hashmap_uint64 *hmap, uint64_t key, void *value)
{
  int ret;
  void *retval = NULL;
  khiter_t i = kh_put(HASHMAP_UINT64, hmap->table, key, &ret);

  if (!ret)
    retval = kh_val(hmap->table, i);

  kh_val(hmap->table, i) = value;

  return (retval);
}


void *hashmap_uint64_remove(struct hashmap_uint64 *hmap, uint64_t key)
{
  void *retval = NULL;
  khiter_t i;

  if ((i = kh_get(HASHMAP_UINT64, hmap->table, key)) != kh_end(hmap->table)) {
    retval = kh_val(hmap->table, i);
    kh_del(HASHMAP_UINT64, hmap->table, i);
  }

  return (retval);
}


void hashmap_uint64_free(struct hashmap_uint64 *hmap)
{
  kh_clear(HASHMAP_UINT64, hmap->table);
  kh_destroy(HASHMAP_UINT64, hmap->table);
  FREE(hmap);
}
