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
#include <errno.h>
#include <time.h>

#include "rpc/db/sb-db.h"
#include "sb-common.h"

#define FUNC_MAX_LEN_NAME 255
#define FUNC_MIN_LEN_NAME 1

static int db_function_add_name(string pluginkey, string name)
{
  redisReply *reply;

  if (!rc)
    return (-1);

  reply = redisCommand(rc, "SADD %s:func:all %s", pluginkey.str, name.str);

  if (reply->type != REDIS_REPLY_INTEGER) {
    LOG_WARNING("Redis failed to add function name to func:all set: %s",
        reply->str);
    freeReplyObject(reply);
    return (-1);
  }

  freeReplyObject(reply);
  return (0);
}


static int db_function_add_meta(string pluginkey, string name, string desc)
{
  redisReply *reply;

  if (!rc)
    return (-1);

  reply = redisCommand(rc, "HSET %s:func:%s:meta desc %s", pluginkey.str,
          name.str, desc.str);

  if (reply->type == REDIS_REPLY_ERROR) {
    LOG_WARNING("Redis failed to store function meta information: %s",
        reply->str);
    freeReplyObject(reply);
    return (-1);
  }

  freeReplyObject(reply);
  return (0);
}


static int db_function_add_args(string pluginkey, string name,
    message_object_type type)
{
  redisReply *reply;

  if (!rc)
    return (-1);

  reply = redisCommand(rc, "LPUSH %s:func:%s:args %lu", pluginkey.str,
          name.str, type);

  if (reply->type != REDIS_REPLY_INTEGER) {
    LOG_WARNING("Redis failed to store function argument: %s", reply->str);
    freeReplyObject(reply);
    return (-1);
  }

  freeReplyObject(reply);
  return (0);
}


static int db_function_flush_args(string pluginkey, string name)
{
  redisReply *reply;
  int start = 1;
  int end = 0;

  if (!rc)
    return (-1);

  /* if start > end, the result will be an empty list (which causes
   * key to be removed) */
  reply = redisCommand(rc, "LTRIM %s:func:%s:args %i %i", pluginkey.str,
          name.str, start, end);

  if ((reply->type != REDIS_REPLY_STATUS) ||
      (strncmp(reply->str, "OK", 3) != 0)) {
    LOG_WARNING("Redis failed to store function argument: %s", reply->str);
    freeReplyObject(reply);

    return (-1);
  }

  freeReplyObject(reply);

  return (0);
}


int db_function_add(string pluginkey, array *func)
{
  struct message_object *name_elem, *desc_elem, *args, *arg;
  string name, desc;

  if (!func || !rc || (func->size <= 2))
    return (-1);

  name_elem = &func->obj[0];

  if (!name_elem) {
    LOG_WARNING("Illegal function name pointer.");
    return (-1);
  }

  name = name_elem->data.string;

  if ((name_elem->type != OBJECT_TYPE_STR) ||
      (name.length < FUNC_MIN_LEN_NAME) ||
      (name.length > FUNC_MAX_LEN_NAME)) {
    LOG_WARNING("Illegal function name.");
    return (-1);
  }

  desc_elem = &func->obj[1];

  if (!desc_elem) {
    LOG_WARNING("Illegal function description pointer.");
    return (-1);
  }

  desc = desc_elem->data.string;

  if (desc_elem->type != OBJECT_TYPE_STR) {
    LOG_WARNING("Illegal function description.");
    return (-1);
  }

  if (db_function_add_name(pluginkey, name) == -1)
    return (-1);

  if (db_function_add_meta(pluginkey, name, desc) == -1)
    return (-1);

  args = &func->obj[2];

  db_function_flush_args(pluginkey, name);

  for (size_t i = 0; i < args->data.params.size; i++) {
    arg = &args->data.params.obj[i];

    if (db_function_add_args(pluginkey, name, arg->type) == -1) {
      LOG_WARNING("Failed to add function arguments!");
      return (-1);
    }
  }

  return (0);
}


static bool db_function_exists(string pluginkey, string name)
{
  redisReply *reply;
  bool result;

  if (!rc) {
    LOG_WARNING("No redis connection available!");
    return (false);
  }

  reply = redisCommand(rc, "SISMEMBER %s:func:all %s", pluginkey.str,
          name.str);

  if (reply->type != REDIS_REPLY_INTEGER) {
    LOG_WARNING("Redis failed to check if function is registered.");
    freeReplyObject(reply);
    return (false);
  }

  result = reply->integer != 0;
  freeReplyObject(reply);

  return (result);
}


static ssize_t db_function_get_argc(string pluginkey, string name)
{
  redisReply *reply;
  ssize_t result;

  if (!rc) {
    LOG_WARNING("No redis connection available!");
    return (-1);
  }

  reply = redisCommand(rc, "LLEN %s:func:%s:args", pluginkey.str,
            name.str);

  if (reply->type != REDIS_REPLY_INTEGER) {
    LOG_WARNING("Redis failed to get arguments list length: %s", reply->str);
    freeReplyObject(reply);
    return (-1);
  }

  result = reply->integer;
  freeReplyObject(reply);

  return (result);
}


static int db_function_typecheck(string pluginkey, string name,
    array *args)
{
  redisReply *reply;
  ssize_t argc = 0;
  char *endptr;
  long val;

  if (!rc) {
    LOG_WARNING("No redis connection available!");
    return (-1);
  }

  if (((argc = db_function_get_argc(pluginkey, name)) < 0) ||
      ((size_t)argc != (size_t)args->size)) {
    LOG_WARNING("Invalid argument count!");
    return (-1);
  }

  reply = redisCommand(rc, "LRANGE %s:func:%s:args 0 %d", pluginkey.str,
            name.str, argc);

  if (reply->type == REDIS_REPLY_ARRAY)
    for (size_t j = reply->elements, k = 0; j != 0; j--, k++) {

      if (reply->element[j-1]->type != REDIS_REPLY_STRING) {
        LOG_WARNING("Redis returned list element has wrong type.");
        freeReplyObject(reply);
        return (-1);
      }

      errno = 0;

      val = strtol(reply->element[j-1]->str, &endptr, 10);

      if (((errno == ERANGE) && ((val == LONG_MAX) || (val == LONG_MIN))) ||
          ((errno != 0) && (val == 0))) {
        LOG_WARNING("Redis function argument has wrong type.");
        freeReplyObject(reply);
        return (-1);
      }

      if (endptr == reply->element[j-1]->str) {
        LOG_WARNING("Redis function argument has wrong type.");
        freeReplyObject(reply);
        return (-1);
      }

      if(val == OBJECT_TYPE_INT && args->obj[k].type == OBJECT_TYPE_UINT){
        /* Any positive integer will be treated as an unsigned int
         * (see unpack/pack.c) and might be a valid signed integer */
        if(args->obj[k].data.uinteger > INT64_MAX){
          LOG_WARNING("run() function argument has wrong type.");
          freeReplyObject(reply);
          return (-1);
        }
      } else if (val != args->obj[k].type){
        LOG_WARNING("run() function argument has wrong type.");
        freeReplyObject(reply);
        return (-1);
      }

    }

  freeReplyObject(reply);

  return (0);
}


int db_function_verify(string pluginkey, string name,
    array *args)
{
  if (!db_function_exists(pluginkey, name))
    return (-1);

  if (db_function_typecheck(pluginkey, name, args) == -1)
    return (-1);

  return (0);
}
