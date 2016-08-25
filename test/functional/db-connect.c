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

#include "rpc/db/sb-db.h"
#include "sb-common.h"
#include "helper-unix.h"

#define DB_PORT 6378

void functional_db_connect(UNUSED(void **state))
{
  struct timeval timeout = { 1, 500000 };

  assert_int_equal(0, db_connect("127.0.0.1", DB_PORT, timeout,
      "vBXBg3Wkq3ESULkYWtijxfS5UvBpWb-2mZHpKAKpyRuTmvdy4WR7cTJqz-vi2BA2"));
  db_close();
  assert_int_not_equal(0, db_connect("127.0.0.2", DB_PORT, timeout,
      "vBXBg3Wkq3ESULkYWtijxfS5UvBpWb-2mZHpKAKpyRuTmvdy4WR7cTJqz-vi2BA2"));
  assert_int_not_equal(0, db_connect("127.0.0.1", 1234, timeout,
      "vBXBg3Wkq3ESULkYWtijxfS5UvBpWb-2mZHpKAKpyRuTmvdy4WR7cTJqz-vi2BA2"));
  assert_int_not_equal(0, db_connect("127.0.0.1", DB_PORT, timeout,
      "wrong password"));
}
