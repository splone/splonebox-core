#include <stdlib.h>
#include <msgpack/unpack.h>
#include <msgpack/object.h>
#include "rpc/sb-rpc.h"
#include "helper-unix.h"
#include "wrappers.h"
#include "sb-common.h"

/* enable/disable wrapper for api_run */
bool WRAP_API_RUN = false;
/* Declare original api_run (__real_api_run) */
struct message_params_object * __real_api_run(string pluginlongtermpk,
    string function_name, uint64_t callid, struct message_params_object args,
                                              struct api_error *api_error);



int __wrap_outputstream_write(UNUSED(outputstream *ostream), char *buffer, size_t len)
{
  msgpack_object deserialized;
  msgpack_zone mempool;
  msgpack_zone_init(&mempool, 2048);

  msgpack_unpack(buffer, len, NULL, &mempool, &deserialized);
  LOG("server sends: ");
  msgpack_object_print(stdout, deserialized);
  LOG("\n");
  check_expected(&deserialized);

  return (0);
}

struct callinfo *__wrap_connection_wait_for_response(UNUSED(struct connection *con),
    UNUSED(struct message_request *request))
{
  struct message_response *response;
  struct callinfo *cinfo;

  cinfo = MALLOC(struct callinfo);
  assert_non_null(cinfo);

  response = MALLOC(struct message_response);
  assert_non_null(response);

  /* The callid and the message_params type are determined by the unit test. */
  response->params.size = 1;
  response->params.obj = CALLOC(response->params.size, struct message_object);
  response->params.obj[0].type = (message_object_type)mock();
  response->params.obj[0].data.uinteger = (uint64_t)mock();

  cinfo->response = response;

  return (cinfo);
}

struct message_params_object * __wrap_api_run(string pluginlongtermpk,
    string function_name, uint64_t callid, struct message_params_object args,
                                              struct api_error *api_error){
  if(WRAP_API_RUN){
          return (struct message_params_object *) mock();
  }

  return __real_api_run(pluginlongtermpk, function_name, callid, args, api_error);
}
