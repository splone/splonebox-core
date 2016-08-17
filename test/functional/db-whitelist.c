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

#include "sb-common.h"
#include "rpc/db/sb-db.h"
#include "helper-all.h"
#include "helper-unix.h"

void functional_db_whitelist(UNUSED(void **state))
{
  connect_to_db();

  assert_false(db_authorized_whitelist_all_is_set());
  assert_int_equal(0, db_authorized_set_whitelist_all());
  assert_true(db_authorized_whitelist_all_is_set());

  db_close();
}

