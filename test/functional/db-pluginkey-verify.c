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
  string invalid;
  string empty;

  string pluginkey = cstring_copy_string("mykey");
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
  invalid = cstring_copy_string("foobar");
  assert_int_not_equal(0, db_plugin_verify(invalid));

  empty = cstring_copy_string("");
  assert_int_not_equal(0, db_plugin_verify(empty));

  free_string(pluginkey);
  free_string(name);
  free_string(desc);
  free_string(author);
  free_string(license);
  free_string(invalid);
  free_string(empty);

  db_close();
}
