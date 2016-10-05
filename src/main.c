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
#include "rpc/db/sb-db.h"

int8_t verbose_level;
uv_loop_t loop;

int main(int argc, char **argv)
{
  options *globaloptions;

  optparser(argc, argv);

  if (options_init_from_boxrc() < 0) {
      LOG_ERROR("Reading config failed--see warnings above. "
              "For usage, try -h.");
      abort();
  }

  uv_loop_init(&loop);

  crypto_init();

  struct timeval timeout = { 1, 500000 };

  globaloptions = options_get();

  /* connect to database */
  if (db_connect(fmt_addr(&globaloptions->RedisDatabaseListenAddr),
      globaloptions->RedisDatabaseListenPort, timeout,
      globaloptions->RedisDatabaseAuth) < 0) {
    LOG_ERROR("Failed to connect to database");
    abort();
  }

  /* initialize signal handler */
  if (signal_init() == -1) {
    LOG_ERROR("Failed to initialize signal handler.");
    abort();
  }

  /* initialize event queue */
  if (event_initialize() == -1) {
    LOG_ERROR("Failed to initialize event queue.");
    abort();
  }

  /* initialize connections */
  if (connection_init() == -1) {
    LOG_ERROR("Failed to initialise connections.");
    abort();
  }

  if (server_init() == -1) {
    LOG_ERROR("Failed to initialise server.");
    abort();
  }

  /* initialize server */
  if (globaloptions->apitype == SERVER_TYPE_TCP) {
    server_start_tcp(&globaloptions->ApiTransportListenAddr,
        globaloptions->ApiTransportListenPort);
  } else if (globaloptions->apitype == SERVER_TYPE_PIPE) {
    server_start_pipe(globaloptions->ApiNamedPipeListen);
  }

  uv_run(&loop, UV_RUN_DEFAULT);

  options_free(globaloptions);

  return (0);
}
