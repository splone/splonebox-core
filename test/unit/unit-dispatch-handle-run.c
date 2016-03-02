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


#include "helper-all.h"
#include "sb-common.h"
#include "rpc/msgpack/sb-msgpack-rpc.h"
#include "rpc/sb-rpc.h"
#include "helper-unix.h"
#include "wrappers.h"

#define CALLID 1
#define PKEY "key"
#define FUNCTION_NAME "foo"

void unit_dispatch_handle_run(UNUSED(void **state)){
  connection_request_event_info info;
  struct message_params_object run_params_valid, run_response_params;
  struct callinfo cinfo;
  struct message_object *temp_mo;
  char *temp_ch;

  /* #### TEST PREPARATION ##### */

  /* enable wrappers */
  WRAP_API_RUN = true;
  WRAP_CONNECTION_SEND_REQUEST = true;
  WRAP_API_RUN_RESPONSE = true;
  WRAP_CONNECTION_SEND_RESPONSE = true;
  WRAP_RANDOMMOD = true;

  /* init dispatch table */
  assert_false(dispatch_table_init());

  /* init connection_request_info */

  info.request = MALLOC(struct message_request);
  assert_non_null(info.request);

  info.api_error.isset = false;

  info.con = MALLOC(struct connection);
  assert_non_null(info.con);

  /* [[PKEY, NIL], FUNCTION_NAME, []] */
  info.request->params.size = 3;
  info.request->params.obj = CALLOC(3, struct message_object);

  info.request->params.obj[0].type = OBJECT_TYPE_ARRAY;
  info.request->params.obj[0].data.params.size = 2;
  info.request->params.obj[0].data.params.obj =
    CALLOC(2, struct message_object);

  info.request->params.obj[0].data.params.obj[0].type = OBJECT_TYPE_STR;
  info.request->params.obj[0].data.params.obj[0].data.string =
    cstring_copy_string(PKEY);

  info.request->params.obj[0].data.params.obj[1].type = OBJECT_TYPE_NIL;

  info.request->params.obj[1].type = OBJECT_TYPE_STR;
  info.request->params.obj[1].data.string = cstring_copy_string(FUNCTION_NAME);

  info.request->params.obj[2].type = OBJECT_TYPE_ARRAY;
  info.request->params.obj[2].data.params.size = 0;

  /* init run_params_valid */
  /* [[NIL, CALLID], FUNCTION_NAME, []] */
  run_params_valid.size = 3;
  run_params_valid.obj = CALLOC(3, struct message_object);
  assert_non_null(run_params_valid.obj);

  run_params_valid.obj[0].type = OBJECT_TYPE_ARRAY;
  run_params_valid.obj[0].data.params.size = 2;
  run_params_valid.obj[0].data.params.obj = CALLOC(2, struct message_object);
  assert_non_null(run_params_valid.obj[0].data.params.obj);

  run_params_valid.obj[0].data.params.obj[0].type = OBJECT_TYPE_NIL;
  run_params_valid.obj[0].data.params.obj[1].type = OBJECT_TYPE_UINT;
  run_params_valid.obj[0].data.params.obj[1].data.uinteger = CALLID;

  run_params_valid.obj[1].type = OBJECT_TYPE_STR;
  run_params_valid.obj[1].data.string = cstring_copy_string(FUNCTION_NAME);

  run_params_valid.obj[2].type = OBJECT_TYPE_ARRAY;
  run_params_valid.obj[2].data.params.size = 0;
  run_params_valid.obj[2].data.params.obj = NULL;



  /* init callinfo */
  cinfo.msgid = 123;
  cinfo.hasresponse = false;
  cinfo.response = MALLOC(struct message_response);
  cinfo.response->params.size = 1;
  cinfo.response->params.obj = CALLOC(1, struct message_object);
  cinfo.response->params.obj[0].type = OBJECT_TYPE_UINT;
  cinfo.response->params.obj[0].data.uinteger = CALLID;

  /* #### TEST EXECUTION ##### */

  /* Valid run request */
  will_return(__wrap_randommod, CALLID);
  will_return(__wrap_api_run, &run_params_valid);
  will_return(__wrap_connection_send_request, &cinfo);
  will_return(__wrap_api_run_response, &run_response_params);
  will_return(__wrap_connection_send_response, 0);
  assert_false(handle_run(&info));

  /* Invalid params */
  info.request->params.size = 2;
  assert_true(handle_run(&info));
  info.request->params.size = 3;

  info.request->params.obj[0].type = OBJECT_TYPE_INT;
  assert_true(handle_run(&info));
  info.request->params.obj[0].type = OBJECT_TYPE_ARRAY;


  /* Invalid Meta */
  temp_mo = info.request->params.obj[0].data.params.obj;
  info.request->params.obj[0].data.params.obj = NULL;
  assert_true(handle_run(&info));
  info.request->params.obj[0].data.params.obj = temp_mo;


  info.request->params.obj[0].data.params.size = 1;
  assert_true(handle_run(&info));
  info.request->params.obj[0].data.params.size = 2;

  info.request->params.obj[0].data.params.obj[0].type = OBJECT_TYPE_INT;
  assert_true(handle_run(&info));
  info.request->params.obj[0].data.params.obj[0].type = OBJECT_TYPE_STR;

  temp_ch = info.request->params.obj[0].data.params.obj[0].data.string.str;
  info.request->params.obj[0].data.params.obj[0].data.string.str = NULL;
  assert_true(handle_run(&info));
  info.request->params.obj[0].data.params.obj[0].data.string.str = temp_ch;

  info.request->params.obj[0].data.params.obj[1].type = OBJECT_TYPE_INT;
  assert_true(handle_run(&info));
  info.request->params.obj[0].data.params.obj[1].type = OBJECT_TYPE_NIL;

  /* Invalid function name */
  info.request->params.obj[1].type = OBJECT_TYPE_INT;
  assert_true(handle_run(&info));
  info.request->params.obj[1].type = OBJECT_TYPE_STR;

  temp_ch = info.request->params.obj[1].data.string.str;
  info.request->params.obj[1].data.string.str = NULL;
  assert_true(handle_run(&info));
  info.request->params.obj[1].data.string.str = temp_ch;

  /* Error in api_run (api_run returns NULL) */
  will_return(__wrap_randommod, CALLID);
  will_return(__wrap_api_run, NULL);
  assert_true(handle_run(&info));

  /* Error in connection_send_request */
  will_return(__wrap_randommod, CALLID);
  will_return(__wrap_api_run, &run_params_valid);
  will_return(__wrap_connection_send_request, NULL);
  assert_true(handle_run(&info));

  /* Invalid response params */
  cinfo.response->params.size = 2;
  will_return(__wrap_randommod, CALLID);
  will_return(__wrap_api_run, &run_params_valid);
  will_return(__wrap_connection_send_request, &cinfo);
  assert_true(handle_run(&info));
  cinfo.response->params.size = 1;

  /* Invalid call_id received */
  will_return(__wrap_randommod, 100);
  will_return(__wrap_api_run, &run_params_valid);
  will_return(__wrap_connection_send_request, &cinfo);
  assert_true(handle_run(&info));

  cinfo.response->params.obj[0].type = OBJECT_TYPE_STR;
  will_return(__wrap_randommod, CALLID);
  will_return(__wrap_api_run, &run_params_valid);
  will_return(__wrap_connection_send_request, &cinfo);
  assert_true(handle_run(&info));
  cinfo.response->params.obj[0].type = OBJECT_TYPE_UINT;

  /* api_run_response fails */
  will_return(__wrap_randommod, CALLID);
  will_return(__wrap_api_run, &run_params_valid);
  will_return(__wrap_connection_send_request, &cinfo);
  will_return(__wrap_api_run_response, NULL);
  assert_true(handle_run(&info));

  /* connection_send_response fails */
  will_return(__wrap_randommod, CALLID);
  will_return(__wrap_api_run, &run_params_valid);
  will_return(__wrap_connection_send_request, &cinfo);
  will_return(__wrap_api_run_response, &run_response_params);
  will_return(__wrap_connection_send_response, -1);
  assert_true(handle_run(&info));

  /* #### TEST CLEANUP ##### */

  WRAP_API_RUN = false;
  WRAP_CONNECTION_SEND_REQUEST = false;
  WRAP_API_RUN_RESPONSE = false;
  WRAP_CONNECTION_SEND_RESPONSE = false;
  WRAP_RANDOMMOD = false;

  /* cleanup */
  free_string(info.request->params.obj[0].data.params.obj[0].data.string);
  free_string(info.request->params.obj[1].data.string);
  free_string(run_params_valid.obj[1].data.string);

  FREE(info.con);
  FREE(info.request->params.obj[0].data.params.obj);
  FREE(info.request->params.obj);
  FREE(info.request);
  FREE(run_params_valid.obj[0].data.params.obj);
  FREE(run_params_valid.obj);
  FREE(cinfo.response->params.obj);
  FREE(cinfo.response);

}
