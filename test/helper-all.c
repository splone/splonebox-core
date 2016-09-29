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

  p->key = cstring_copy_string("0123456789ABCDEF");
  p->name = cstring_copy_string("plugin name");
  p->description = cstring_copy_string("plugin desc");
  p->license = cstring_copy_string("plugin license");
  p->author = cstring_copy_string("plugin author");
  p->callid = 0;

  f->name = cstring_copy_string("function name");
  f->description = cstring_copy_string("function desc");
  f->args[0] = cstring_copy_string("arg 1");
  f->args[1] = cstring_copy_string("arg 2");

  p->function = f;
  return p;
}

void helper_free_plugin(struct plugin *p)
{
  free_string(p->key);
  free_string(p->name);
  free_string(p->description);
  free_string(p->license);
  free_string(p->author);

  free_string(p->function->name);
  free_string(p->function->description);
  free_string(p->function->args[0]);
  free_string(p->function->args[1]);

  FREE(p->function);
  FREE(p);
}


/* Builds a register call and executes handle_register in order
 * to register the plugin. */
void helper_register_plugin(struct plugin *p)
{
  connection_request_event_info info;
  struct message_request *register_request;
  array *meta, *functions, *func1, *args;
  struct api_error err = ERROR_INIT;

  info.request.msgid = 1;
  register_request = &info.request;
  assert_non_null(register_request);

  info.api_error = err;

  info.con = MALLOC(struct connection);
  info.con->closed = true;
  info.con->msgid = 1;
  assert_non_null(info.con);

  strlcpy(info.con->cc.pluginkeystring, p->key.str, p->key.length+1);

  connect_to_db();
  assert_int_equal(0, connection_init());

  /* first level arrays:
   *
   * [meta] args->obj[0]
   * [functions] args->obj[1]
   */
  register_request->params.size = 2;
  register_request->params.obj = CALLOC(2, struct message_object);
  register_request->params.obj[0].type = OBJECT_TYPE_ARRAY;
  register_request->params.obj[1].type = OBJECT_TYPE_ARRAY;

  /* meta array:
   *
   * [name]
   * [desciption]
   * [author]
   * [license]
   */
   meta = &register_request->params.obj[0].data.params;
   meta->size = 4;
   meta->obj = CALLOC(meta->size, struct message_object);
   meta->obj[0].type = OBJECT_TYPE_STR;
   meta->obj[0].data.string = cstring_copy_string(p->name.str);
   meta->obj[1].type = OBJECT_TYPE_STR;
   meta->obj[1].data.string = cstring_copy_string(p->description.str);
   meta->obj[2].type = OBJECT_TYPE_STR;
   meta->obj[2].data.string = cstring_copy_string(p->author.str);
   meta->obj[3].type = OBJECT_TYPE_STR;
   meta->obj[3].data.string = cstring_copy_string(p->license.str);

  functions = &register_request->params.obj[1].data.params;
  functions->size = 1;
  functions->obj = CALLOC(functions->size, struct message_object);
  functions->obj[0].type = OBJECT_TYPE_ARRAY;

  func1 = &functions->obj[0].data.params;
  func1->size = 3;
  func1->obj = CALLOC(3, struct message_object);
  func1->obj[0].type = OBJECT_TYPE_STR;
  func1->obj[0].data.string = cstring_copy_string(p->function->name.str);
  func1->obj[1].type = OBJECT_TYPE_STR;
  func1->obj[1].data.string = cstring_copy_string(p->function->description.str);
  func1->obj[2].type = OBJECT_TYPE_ARRAY;

  /* function arguments */
  args = &func1->obj[2].data.params;
  args->size = 2;
  args->obj = CALLOC(2, struct message_object);
  args->obj[0].type = OBJECT_TYPE_STR;
  args->obj[0].data.string = cstring_copy_string(p->function->args[0].str);
  args->obj[1].type = OBJECT_TYPE_STR;
  args->obj[1].data.string = cstring_copy_string(p->function->args[1].str);

  /* before running function, it must be registered successfully */
  info.api_error.isset = false;
  expect_check(__wrap_crypto_write, &deserialized,
                validate_register_response,
                NULL);
  assert_int_equal(0, handle_register(&info));
  assert_false(info.api_error.isset);

  connection_hashmap_put(info.con->cc.pluginkeystring, info.con);

  FREE(args->obj);
  FREE(func1->obj);
  FREE(functions->obj);
  FREE(meta->obj);
  FREE(register_request->params.obj);
}

/* Builds a run request */
void helper_build_run_request(struct message_request *rr,
    struct plugin *plugin,
    message_object_type metaarraytype,
    size_t metasize,
    message_object_type metaclientidtype,
    message_object_type metacallidtype,
    message_object_type functionnametype,
    message_object_type argstype
)
{
  array argsarray = ARRAY_INIT;
  array *meta;

  rr->msgid = 1;

  rr->params.size = 3;
  rr->params.obj = CALLOC(rr->params.size, struct message_object);
  rr->params.obj[0].type = metaarraytype;

  meta = &rr->params.obj[0].data.params;
  meta->size = metasize;
  meta->obj = CALLOC(meta->size, struct message_object);
  meta->obj[0].type = metaclientidtype;
  meta->obj[0].data.string = cstring_copy_string(plugin->key.str);
  meta->obj[1].type = metacallidtype;

  rr->params.obj[1].type = functionnametype;
  rr->params.obj[1].data.string = cstring_copy_string(plugin->function->name.str);

  argsarray.size = 2;
  argsarray.obj = CALLOC(argsarray.size, struct message_object);
  argsarray.obj[0].type = OBJECT_TYPE_STR; /* first argument */
  argsarray.obj[0].data.string = cstring_copy_string(plugin->function->args[0].str);
  argsarray.obj[1].type = OBJECT_TYPE_STR; /* second argument */
  argsarray.obj[1].data.string = cstring_copy_string(plugin->function->args[1].str);

  rr->params.obj[2].type = argstype;
  rr->params.obj[2].data.params = argsarray;
}


void helper_build_result_request(struct message_request *rr,
    struct plugin *plugin,
    message_object_type metaarraytype,
    size_t metasize,
    message_object_type metacallidtype,
    message_object_type argstype)
{
  array argsarray = ARRAY_INIT;
  array *meta;

  rr->msgid = 1;

  rr->params.size = 2;
  rr->params.obj = CALLOC(rr->params.size, struct message_object);
  rr->params.obj[0].type = metaarraytype;
  meta = &rr->params.obj[0].data.params;
  meta->size = metasize;
  meta->obj = CALLOC(meta->size, struct message_object);
  meta->obj[0].type = metacallidtype;
  meta->obj[0].data.uinteger = plugin->callid;

  argsarray.size = 2;
  argsarray.obj = CALLOC(argsarray.size, struct message_object);
  argsarray.obj[0].type = OBJECT_TYPE_STR; /* first argument */
  argsarray.obj[0].data.string = cstring_copy_string(plugin->function->args[0].str);
  argsarray.obj[1].type = OBJECT_TYPE_STR; /* second argument */
  argsarray.obj[1].data.string = cstring_copy_string(plugin->function->args[1].str);

  rr->params.obj[1].type = argstype;
  rr->params.obj[1].data.params = argsarray;
}

void helper_request_set_callid(struct message_request *rr,
  message_object_type callidtype)
{
  array *meta = &rr->params.obj[0].data.params;

  meta->obj[0].type = callidtype;
}

void helper_request_set_meta_size(struct message_request *rr,
  message_object_type metatype, uint64_t metasize)
{
  rr->params.obj[0].type = metatype;
  rr->params.obj[0].data.params.size = metasize;
}

void helper_request_set_args_size(struct message_request *rr,
  message_object_type argstype, uint64_t argssize)
{
  rr->params.obj[1].type = argstype;
  rr->params.obj[1].data.params.size = argssize;
}

void helper_request_set_function_name(struct message_request *rr,
  message_object_type nametype, char *name)
{
  free_string(rr->params.obj[1].data.string);
  rr->params.obj[1].type = nametype;
  rr->params.obj[1].data.string = cstring_copy_string(name);
}

void helper_request_set_pluginkey_type(struct message_request *rr,
  message_object_type pluginkeytype, char *pluginkey)
{
  array *meta = &rr->params.obj[0].data.params;

  free_string(meta->obj[0].data.string);
  meta->obj[0].data.string = cstring_copy_string(pluginkey);
  meta->obj[0].type = pluginkeytype;
}
