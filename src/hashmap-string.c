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

#include "sb-common.h"

struct hashmap_string *hashmap_string_new()
{
  struct hashmap_string *hmap = MALLOC(struct hashmap_string);

  if (hmap == NULL)
    return (NULL);

  hmap->table = kh_init(HASHMAP_STRING);

  return (hmap);
}


void *hashmap_string_get(struct hashmap_string *hmap, string key)
{
  khiter_t i;

  if ((i = kh_get(HASHMAP_STRING, hmap->table, key)) == kh_end(hmap->table))
    return (NULL);

  return (kh_val(hmap->table, i));
}


bool hashmap_string_contains_key(struct hashmap_string *hmap, string key)
{
  return (kh_get(HASHMAP_STRING, hmap->table, key) != kh_end(hmap->table));
}


void *hashmap_string_put(struct hashmap_string *hmap, string key, void *value)
{
  int ret;
  void *retval = NULL;
  khiter_t i = kh_put(HASHMAP_STRING, hmap->table, key, &ret);

  if (!ret)
    retval = kh_val(hmap->table, i);

  kh_val(hmap->table, i) = value;

  return (retval);
}


void *hashmap_string_remove(struct hashmap_string *hmap, string key)
{
  void *retval = NULL;
  khiter_t i;

  if ((i = kh_get(HASHMAP_STRING, hmap->table, key)) != kh_end(hmap->table)) {
    retval = kh_val(hmap->table, i);
    kh_del(HASHMAP_STRING, hmap->table, i);
  }

  return (retval);
}


void hashmap_string_free(struct hashmap_string *hmap)
{
  kh_clear(HASHMAP_STRING, hmap->table);
  kh_destroy(HASHMAP_STRING, hmap->table);
  FREE(hmap);
}
