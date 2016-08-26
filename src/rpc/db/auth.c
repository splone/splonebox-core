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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "rpc/db/sb-db.h"
#include "sb-common.h"

#define DB_AUTH_WHITELIST_ALL_SYM "*"

int db_authorized_add(unsigned char *pluginlongtermpk)
{
  redisReply *reply;

  reply = redisCommand(rc, "SADD authorized %b ", pluginlongtermpk, CLIENTLONGTERMPK_ARRAY_SIZE);

  if (reply->type == REDIS_REPLY_ERROR) {
    LOG_WARNING("Redis failed to add string value to plugin: %s", reply->str);
    freeReplyObject(reply);
    return (-1);
  }

  freeReplyObject(reply);

  return (0);
}

bool db_authorized_verify(unsigned char *pluginlongtermpk)
{
  redisReply *reply;
  bool valid = false;

  reply = redisCommand(rc, "SISMEMBER authorized %b", pluginlongtermpk,
    CLIENTLONGTERMPK_ARRAY_SIZE);

  if (reply->type != REDIS_REPLY_INTEGER)
    LOG_WARNING("Redis failed to query plugin key existence: %s", reply->str);
  else
    valid = reply->integer == 1;

  freeReplyObject(reply);

  return valid;
}

int db_authorized_set_whitelist_all(void)
{
  redisReply *reply;

  reply = redisCommand(rc, "SADD authorized %s ", DB_AUTH_WHITELIST_ALL_SYM);

  if (reply->type == REDIS_REPLY_ERROR) {
    LOG_WARNING("Redis failed to add string value to plugin: %s", reply->str);
    freeReplyObject(reply);
    return (-1);
  }

  freeReplyObject(reply);

  return (0);

}

bool db_authorized_whitelist_all_is_set(void)
{
  redisReply *reply;
  bool valid = false;

  reply = redisCommand(rc, "SISMEMBER authorized %s", DB_AUTH_WHITELIST_ALL_SYM);

  if (reply->type != REDIS_REPLY_INTEGER)
    LOG_WARNING("Redis failed to query plugin key existence: %s", reply->str);
  else
    valid = reply->integer == 1;

  freeReplyObject(reply);

  return valid;
}



