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
#include <sodium.h>

#include "sb-common.h"
#include "rpc/sb-rpc.h"

#define ENV_VAR_LISTEN_ADDRESS    "SPLONEBOX_LISTEN_ADDRESS"

#define DB_IP "127.0.0.1"
#define DB_PORT 6378
#define DB_PASSWORD "vBXBg3Wkq3ESULkYWtijxfS5UvBpWb-2mZHpKAKpyRuTmvdy4WR7cTJqz-vi2BA2"

int8_t verbose_level;
uv_loop_t loop;

int main(int argc, char **argv)
{
  optparser(argc, argv);

  uv_loop_init(&loop);

  string db_ip = cstring_copy_string(DB_IP);
  string db_auth = cstring_copy_string(DB_PASSWORD);
  struct timeval timeout = { 1, 500000 };

  if (db_connect(db_ip, DB_PORT, timeout, db_auth) < 0) {
    LOG_ERROR("Failed to connect to database");
    return EXIT_FAILURE;
  }

  /* initialize signal handler */
  if (signal_init() == -1) {
    LOG_ERROR("Failed to initialize signal handler.");
  }

  /* initialize libsodium */
  if (sodium_init() == -1) {
    LOG_ERROR("Failed to initialize sodium.");
  }

  /* initialize event queue */
  if (event_initialize() == -1) {
    LOG_ERROR("Failed to initialize event queue.");
  }

  /* initialize connections */
  if (connection_init() == -1) {
    LOG_ERROR("Failed to initialise connections.");
  }

  /* initialize server */
  string env = cstring_to_string(getenv(ENV_VAR_LISTEN_ADDRESS));

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
