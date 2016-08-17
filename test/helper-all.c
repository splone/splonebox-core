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
#pragma once
#include <hiredis/hiredis.h>

#include "sb-common.h"
#include "rpc/db/sb-db.h"
#include "helper-unix.h"

void connect_to_db(void)
{
  redisReply *reply;
  options *globaloptions;
  struct timeval timeout = { 1, 500000 };

  if (options_init_from_boxrc() < 0) {
      LOG_ERROR("Reading config failed--see warnings above. "
              "For usage, try -h.");
      return;
  }

  globaloptions = options_get();

  /* connect to database */
  assert_int_equal(0, db_connect(
      fmt_addr(&globaloptions->RedisDatabaseListenAddr),
      globaloptions->RedisDatabaseListenPort, timeout,
      globaloptions->RedisDatabaseAuth));

  reply = redisCommand(rc, "FLUSHALL");

  freeReplyObject(reply);

  options_free(globaloptions);
}

void connect_and_create(string pluginkey)
{
  string name = cstring_copy_string("my new plugin");
  string desc = cstring_copy_string("Lorem ipsum");
  string author = cstring_copy_string("author of the plugin");
  string license = cstring_copy_string("license foobar");

  connect_to_db();

  assert_int_equal(0, db_plugin_add(pluginkey, name, desc, author, license));

  free_string(name);
  free_string(desc);
  free_string(author);
  free_string(license);
}
