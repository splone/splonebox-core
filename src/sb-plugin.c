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

#include "rpc/db/sb-db.h"
#include "api/sb-api.h"
#include "sb-common.h"

#define DB_PORT 6378
#define DB_IP "127.0.0.1"
#define DB_AUTH "vBXBg3Wkq3ESULkYWtijxfS5UvBpWb-2mZHpKAKpyRuTmvdy4WR7cTJqz-vi2BA2"

#define API_KEY_SIZE 64

#define CMD_ADD "add"

int8_t verbose_level;

static void print_usage(const char *name)
{
  LOG("Usage: %s [add]\n\n"
      "Manages plugins.\n\n"
      "Commands:\n"
      "  add  add a new API key to database\n",
      name);
}

static int add_plugin(void)
{
  string apikey;
  struct timeval timeout = { 1, 500000 };
  string db_ip = cstring_copy_string(DB_IP);
  string db_auth = cstring_copy_string(DB_AUTH);

  apikey.str = MALLOC_ARRAY(API_KEY_SIZE, char);
  if (!apikey.str)
    LOG_ERROR("Failed to allocate mem for api key string.");
  apikey.length = API_KEY_SIZE;

  if (api_get_key(apikey) < 0)
    LOG_ERROR("Failed to generate api key!\n");

  if (db_connect(DB_IP, DB_PORT, timeout, DB_AUTH) < 0)
    LOG_ERROR("Failed to connect do database!\n");

  if (db_apikey_add(apikey) < 0)
    LOG_ERROR("Failed to store api key in database!\n");

  LOG("%s\n", apikey.str);

  free_string(db_ip);
  free_string(apikey);
  free_string(db_auth);
  db_close();

  return (0);
}

int main(int argc, char **argv)
{
  if (argc <= 1) {
    print_usage(argv[0]);
    return (0);
  }

  if (strncmp(argv[1], CMD_ADD, strlen(CMD_ADD)) == 0)
    add_plugin();
  else
    return (-1);

  return (0);
}
