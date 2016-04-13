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
#include "tweetnacl.h"
#include "helper-unix.h"

unsigned char data[32];

void functional_filesystem_load(UNUSED(void **state))
{
  assert_int_equal(0, mkdir(".test-functional-fs-load", 0700));

  randombytes(data, sizeof(data));

  assert_int_equal(0, filesystem_save_sync(".test-functional-fs-load/data",
      data, sizeof(data)));

  /* positiv test */
  assert_int_equal(0, filesystem_load(".test-functional-fs-load/data", data,
      sizeof(data)));

  /* negativ tests */
  assert_int_not_equal(0, filesystem_load(".test-functional-fs-load/data", data,
      48));
  assert_int_not_equal(0, filesystem_load(NULL, data, sizeof(data)));
  assert_int_not_equal(0, filesystem_load(".test-functional-fs-load/data", NULL,
      sizeof(data)));


  /* cleanup */
  assert_int_equal(0, remove(".test-functional-fs-load/data"));
  assert_int_equal(0, remove(".test-functional-fs-load"));
}
