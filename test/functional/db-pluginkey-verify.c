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

#include <stdbool.h>

#include "helper-all.h"
#include "rpc/sb-rpc.h"
#include "sb-common.h"
#include "helper-unix.h"
#include "rpc/db/sb-db.h"
#include "api/sb-api.h"

void functional_db_pluginkey_verify(UNUSED(void **state))
{
  char pluginkey[PLUGINKEY_STRING_SIZE] = "012345789ABCDEFH";
  char invalid_pluginkey[PLUGINKEY_STRING_SIZE] = "FFFFFFFFFFFFFFFF";
  char empty_pluginkey[1] = "";

  string name = cstring_copy_string("myname");
  string desc = cstring_copy_string("mydesc");
  string author = cstring_copy_string("myauthor");
  string license = cstring_copy_string("mylicense");

  connect_to_db();

  assert_int_equal(0, db_plugin_add(
    pluginkey, name, desc, author, license));

  /* verify correct key should succeed */
  assert_int_equal(0, db_plugin_verify(pluginkey));

  /* verify incorrect keys should fail */
  assert_int_not_equal(0, db_plugin_verify(invalid_pluginkey));

  assert_int_not_equal(0, db_plugin_verify(empty_pluginkey));

  free_string(name);
  free_string(desc);
  free_string(author);
  free_string(license);

  db_close();
}
