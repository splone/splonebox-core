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

#include "sb-common.h"
#include "rpc/db/sb-db.h"
#include "api/helpers.h"
#include "api/sb-api.h"

#include "helper-unix.h"
#include "helper-all.h"
#include "helper-validate.h"

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

void connect_and_create(char *pluginkey)
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

struct plugin *helper_get_example_plugin(void)
{
  struct plugin *p = MALLOC(struct plugin);
  struct function *f = MALLOC(struct function);

  if (!p || !f)
    LOG_ERROR("[test] Failed to alloc mem for example plugin.\n");

  p->key = (string) {.str = "0123456789ABCDEF",
      .length = sizeof("0123456789ABCDEF") - 1};
  p->name = (string) {.str = "plugin name",
      .length = sizeof("plugin name") - 1};
  p->description = (string) {.str = "plugin desc",
      .length = sizeof("plugin desc") - 1};
  p->license = (string) {.str = "plugin license",
      .length = sizeof("plugin license") - 1};
  p->author = (string) {.str = "plugin author",
      .length = sizeof("plugin author") - 1};

  p->callid = 0;

  f->name = (string) {.str = "function name",
      .length = sizeof("function name") - 1};
  f->description = (string) {.str = "function desc",
      .length = sizeof("function desc") - 1};
  f->args[0] = (string) {.str = "arg 1",
      .length = sizeof("arg 1") - 1};
  f->args[1] = (string) {.str = "arg 2",
      .length = sizeof("arg 2") - 1};

  p->function = f;
  return p;
}

void helper_free_plugin(struct plugin *p)
{
  FREE(p->function);
  FREE(p);
}
