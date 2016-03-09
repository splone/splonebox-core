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
#define MIN_LEN_APIKEY 32


int db_plugin_add(string apikey, string name, string desc, string author,
    string license)
{
  redisReply *reply;

  LOG_VERBOSE(VERBOSE_LEVEL_0, "adding plugin..");

  if (!rc)
    return (-1);

  if (name.length < MIN_LEN_NAME) {
    LOG_WARNING("Name length should be greater than %d.", MIN_LEN_NAME);
    return (-1);
  }

  reply = redisCommand(rc, "HMSET %s name %s desc %s author %s license %s",
          apikey.str, name.str, desc.str, author.str, license.str);

  if (reply->type == REDIS_REPLY_ERROR) {
    LOG_WARNING("Redis failed to add string value to plugin: %s", reply->str);
    freeReplyObject(reply);
    return (-1);
  }

  freeReplyObject(reply);
  LOG_VERBOSE(VERBOSE_LEVEL_0, ANSI_COLOR_GREEN "done\n" ANSI_COLOR_RESET);
  return (0);
}


int db_apikey_verify(string apikey)
{
  redisReply *reply;
  bool valid = false;

  if (!rc)
    return (-1);

  reply = redisCommand(rc, "Exists %s", apikey.str);

  if (reply->type != REDIS_REPLY_INTEGER)
    LOG_WARNING("Redis failed to query api key existence: %s", reply->str);
  else
    valid = reply->integer == 1;

  freeReplyObject(reply);

  if (!valid)
    return -1;

  return (0);
}


int db_apikey_add(string apikey)
{
  redisReply *reply;

  if (!rc)
    return (-1);

  if (apikey.length < MIN_LEN_APIKEY) {
    LOG_WARNING("API key length should be greater than %d.", MIN_LEN_APIKEY);
    return (-1);
  }

  reply = redisCommand(rc, "HMSET %s  name ''",apikey.str);

  if (reply->type == REDIS_REPLY_ERROR) {
    LOG_WARNING("Redis failed to add string value to plugin: %s", reply->str);
    freeReplyObject(reply);
    return (-1);
  }

  freeReplyObject(reply);

  return (0);
}
