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
#include "main.h"

static void signal_sigint_cb(uv_signal_t *uvhandle, int signum);

uv_signal_t sigint;

int signal_init(void)
{
  /* SIGINT */
  if (uv_signal_init(&main_loop.uv, &sigint) != 0) {
    return (-1);
  }

  if (uv_signal_start(&sigint, signal_sigint_cb, SIGINT) != 0) {
    return (-1);
  }

  return 0;
}

static void signal_sigint_cb(UNUSED(uv_signal_t *handle), UNUSED(int signum))
{
  exit(0);
}
