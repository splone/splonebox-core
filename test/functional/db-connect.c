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

#include "rpc/sb-rpc.h"
#include "sb-common.h"
#include "helper-unix.h"

#define DB_PORT 6378

void functional_db_connect(UNUSED(void **state))
{
  struct timeval timeout = { 1, 500000 };
  string db_ip_one = cstring_copy_string("127.0.0.1");
  string db_ip_two = cstring_copy_string("127.0.0.2");
  string db_auth = cstring_copy_string(
      "vBXBg3Wkq3ESULkYWtijxfS5UvBpWb-2mZHpKAKpyRuTmvdy4WR7cTJqz-vi2BA2");
  string password = cstring_copy_string("wrong password");

  assert_int_equal(0, db_connect(db_ip_one, DB_PORT, timeout, db_auth));
  db_close();
  assert_int_not_equal(0, db_connect(db_ip_two, DB_PORT, timeout, db_auth));
  assert_int_not_equal(0, db_connect(db_ip_one, 1234, timeout, db_auth));
  assert_int_not_equal(0, db_connect(db_ip_one, DB_PORT, timeout, password));

  free_string(db_ip_one);
  free_string(db_ip_two);
  free_string(db_auth);
  free_string(password);
}
