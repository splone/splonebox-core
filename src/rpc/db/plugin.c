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

#define MIN_LEN_NAME 3


int db_plugin_add(unsigned char *pluginkey, string name, string desc, string author,
    string license)
{
  redisReply *reply;

  LOG_VERBOSE(VERBOSE_LEVEL_0, "adding plugin..");

  if (!rc)
    return (-1);

  if (name.length < MIN_LEN_NAME) {
    LOG_WARNING("Name length should be greater than %d.\n", MIN_LEN_NAME);
    return (-1);
  }

  reply = redisCommand(rc, "HMSET %b name %s desc %s author %s license %s",
          pluginkey, PLUGINKEY_ARRAY_SIZE, name.str, desc.str, author.str, license.str);

  if (reply->type == REDIS_REPLY_ERROR) {
    LOG_WARNING("Redis failed to add string value to plugin: %s\n", reply->str);
    freeReplyObject(reply);
    return (-1);
  }

  freeReplyObject(reply);
  LOG_VERBOSE(VERBOSE_LEVEL_0, ANSI_COLOR_GREEN "done\n" ANSI_COLOR_RESET);
  return (0);
}


int db_plugin_verify(unsigned char *pluginkey)
{
  redisReply *reply;
  bool valid = false;

  if (!rc)
    return (-1);

  reply = redisCommand(rc, "Exists %b", pluginkey, PLUGINKEY_ARRAY_SIZE);

  if (reply->type != REDIS_REPLY_INTEGER)
    LOG_WARNING("Redis failed to query plugin key existence: %s", reply->str);
  else
    valid = reply->integer == 1;

  freeReplyObject(reply);

  if (!valid)
    return -1;

  return (0);
}
