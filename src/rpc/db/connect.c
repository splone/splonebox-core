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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "rpc/db/sb-db.h"
#include "sb-common.h"

redisContext *rc;

int db_connect(string ip, int port, const struct timeval tv, string password)
{
  redisReply *reply;
  LOG_VERBOSE(VERBOSE_LEVEL_0, "Connection to database at port %d.\n", port);

  rc = redisConnectWithTimeout(ip.str, port, tv);

  if ((rc == NULL) || rc->err) {
    if (rc) {
      LOG_WARNING("Redis connection error: %s", rc->errstr);
      redisFree(rc);
    } else
      LOG_WARNING("Redis connection error: can't allocate redis context");

    return (-1);
  }

  /* AUTH */
  reply = redisCommand(rc, "AUTH %s", password.str);

  if (reply->type == REDIS_REPLY_ERROR) {
    LOG_WARNING("Redis authentication error: %s", reply->str);
    freeReplyObject(reply);
    redisFree(rc);

    return (-1);
  }

  freeReplyObject(reply);

  return (0);
}


void db_close(void)
{
  redisFree(rc);
}
