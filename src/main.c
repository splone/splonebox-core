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

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "sb-common.h"
#include "tweetnacl.h"
#include "rpc/sb-rpc.h"

#define ENV_VAR_LISTEN_ADDRESS    "SPLONEBOX_LISTEN_ADDRESS"

int8_t verbose_level;
uv_loop_t loop;

int main(int argc, char **argv)
{
  options *globaloptions;

  optparser(argc, argv);

  if (options_init_from_boxrc() < 0) {
      LOG_ERROR("Reading config failed--see warnings above. "
              "For usage, try -h.");
      return -1;
  }

  uv_loop_init(&loop);

  crypto_init();

  /* initialize signal handler */
  if (signal_init() == -1) {
    LOG_ERROR("Failed to initialize signal handler.");
  }

  /* initialize event queue */
  if (event_initialize() == -1) {
    LOG_ERROR("Failed to initialize event queue.");
  }

  /* initialize connections */
  if (connection_init() == -1) {
    LOG_ERROR("Failed to initialise connections.");
  }

  globaloptions = options_get();

  /* initialize server */
  string env = cstring_to_string(globaloptions->ApiListenAddress);

  if (!env.str) {
    LOG_ERROR(
      "Environment Variable 'SPLONEBOX_LISTEN_ADDRESS' is not set, abort.");
  }

  if (server_init() == -1) {
    LOG_ERROR("Failed to initialise server.");
  }

  if (server_start(env) == -1) {
    LOG_ERROR("Failed to start server.");
  }

  uv_run(&loop, UV_RUN_DEFAULT);

  return (0);
}
