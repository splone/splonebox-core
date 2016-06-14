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

#include <time.h>
#include <stdlib.h>

#include "sb-common.h"
#include "rpc/sb-rpc.h"
#include "helper-unix.h"

uv_loop_t loop;

void unit_server_start(UNUSED(void **state))
{
  char buf1[19];
  char buf2[19];
  char buf3[19];

  boxaddr addr;
  uint16_t port;

  box_addr_port_lookup("127.0.0.1:11111", &addr, &port);

  srand((unsigned)time(NULL));
  assert_int_equal(0, server_init());
  uv_loop_init(&loop);

  snprintf(buf1, 19, "/tmp/splnbx-%d", rand() % (999999 + 1 - 100000) + 100000);
  assert_int_equal(0, server_start_pipe(buf1));
  assert_int_equal(0, server_stop(buf1));

  snprintf(buf2, 19, "/tmp/splnbx-%d", rand() % (999999 + 1 - 100000) + 100000);
  assert_int_equal(0, server_start_pipe(buf2));
  assert_int_equal(0, server_stop(buf2));

  snprintf(buf3, 19, "/tmp/splnbx-%d", rand() % (999999 + 1 - 100000) + 100000);
  assert_int_equal(0, server_start_pipe(buf3));
  assert_int_equal(0, server_stop(buf3));

  assert_int_equal(0, server_start_tcp(&addr, port));
  assert_int_equal(0, server_stop((char*)fmt_addr(&addr)));

  server_close();

  uv_run(&loop, UV_RUN_ONCE);
  uv_loop_close(&loop);
}
