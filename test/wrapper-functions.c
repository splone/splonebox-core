#include <stdlib.h>
#include <msgpack/unpack.h>
#include <msgpack/object.h>
#include "rpc/sb-rpc.h"
#include "helper-unix.h"

int __wrap_outputstream_write(UNUSED(outputstream *ostream), char *buffer, size_t len)
{
  msgpack_object deserialized;
  msgpack_zone mempool;
  msgpack_zone_init(&mempool, 2048);

  msgpack_unpack(buffer, len, NULL, &mempool, &deserialized);
//  msgpack_object_print(stdout, deserialized);
  check_expected(&deserialized);

  return (0);
}

struct callinfo *__wrap_connection_wait_for_response(UNUSED(struct connection *con),
    UNUSED(struct message_request *request))
{
  //TODO construct connection info
  LOG("__wrap_wait_for_response\n");

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
