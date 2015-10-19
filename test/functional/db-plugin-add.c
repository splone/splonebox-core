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

#include <hiredis/hiredis.h>

#include "helper-all.h"
#include "rpc/sb-rpc.h"
#include "sb-common.h"
#include "helper-unix.h"


void functional_db_plugin_add(UNUSED(void **state))
{
  /* valid parameters */
  string apikey = cstring_copy_string(
      "bkAhXpRUwuwdeTx0tc24xXDPl6RdIH1uVgQUGRhAZTTjdYiYqkmTmVXgZmRSWuKi");
  string name = cstring_copy_string("my new plugin");
  string desc = cstring_copy_string(
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec a diam "
    "lectus. Sed sit amet ipsum mauris. Maecenas congue ligula ac quam viverra "
    "nec consectetur ante hendrerit. Donec et mollis dolor. Praesent et diam "
    "eget libero egestas mattis sit amet vitae augue. Nam tincidunt congue "
    "enim, ut porta lorem lacinia consectetur. Donec ut libero sed arcu "
    "vehicula ultricies a non tortor. Lorem ipsum dolor sit amet, consectetur "
    "adipiscing elit. Aenean ut gravida lorem. Ut turpis felis, pulvinar a "
    "semper sed, adipiscing id dolor. Pellentesque auctor nisi id magna "
    "consequat sagittis. Curabitur dapibus enim sit amet elit pharetra "
    "tincidunt feugiat nisl imperdiet. Ut convallis libero in urna ultrices "
    "accumsan. Donec sed odio eros. Donec viverra mi quis quam pulvinar at "
    "malesuada arcu rhoncus. Cum sociis natoque penatibus et magnis dis "
    "parturient montes, nascetur ridiculus mus. In rutrum accumsan ultricies. "
    "Mauris vitae nisi at sem facilisis semper ac in est.");
  string author = cstring_copy_string("author of the plugin");
  string license = cstring_copy_string("license foobar");

  connect_to_db();

  assert_int_equal(0, db_plugin_add(apikey, name, desc, author, license));

  /* missing name - must fail */
  string name_empty = cstring_copy_string("");
  assert_int_not_equal(0, db_plugin_add(apikey, name_empty, desc, author,
      license));

  /* missing desc - OK */
  string desc_empty = cstring_copy_string("");
  assert_int_equal(0, db_plugin_add(apikey, name, desc_empty, author,
      license));

  /* missing author - OK */
  string author_empty = cstring_copy_string("");
  assert_int_equal(0, db_plugin_add(apikey, name, desc, author_empty,
      license));

  /* missing license - OK */
  string license_empty = cstring_copy_string("");
  assert_int_equal(0, db_plugin_add(apikey, name, desc, author, license_empty));

  db_close();

  free_string(apikey);
  free_string(name);
  free_string(desc);
  free_string(author);
  free_string(license);
  free_string(name_empty);
  free_string(desc_empty);
  free_string(author_empty);
  free_string(license_empty);
}
