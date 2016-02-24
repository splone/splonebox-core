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

struct callinfo *__wrap_wait_for_response(struct connection *con,
    struct message_request *request)
{
  //TODO construct connection info
  LOG("__wrap_wait_for_response\n");
  return 1;
}
