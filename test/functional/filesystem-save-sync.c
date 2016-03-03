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
#include "sodium.h"
#include "helper-unix.h"

unsigned char data[32];

void functional_filesystem_save_sync(UNUSED(void **state))
{
  assert_int_equal(0, mkdir(".test-functional-fs-save-sync", 0700));

  randombytes(data, sizeof(data));

  /* positiv test */
  assert_int_equal(0, filesystem_save_sync(".test-functional-fs-save-sync/data",
      data, sizeof(data)));

  /* negativ tests */
  assert_int_not_equal(0, filesystem_save_sync(NULL, data, sizeof(data)));
  assert_int_not_equal(0, filesystem_save_sync(
      ".test-functional-fs-save-sync/data", NULL, sizeof(data)));

  /* cleanup */
  assert_int_equal(0, remove(".test-functional-fs-save-sync/data"));
  assert_int_equal(0, remove(".test-functional-fs-save-sync"));
}
