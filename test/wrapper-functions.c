#include <stdlib.h>
#include <msgpack/unpack.h>
#include <msgpack/object.h>
#include "rpc/sb-rpc.h"
#include "helper-unix.h"

bool wrapping_hashmap_string_get = false;

int __wrap_outputstream_write(UNUSED(outputstream *ostream), char *buffer, size_t len)
{
  msgpack_object deserialized;
  msgpack_zone mempool;
  msgpack_zone_init(&mempool, 2048);

  msgpack_unpack(buffer, len, NULL, &mempool, &deserialized);
  msgpack_object_print(stdout, deserialized);
  check_expected(&deserialized);

  return (0);
}
