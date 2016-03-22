#include <stdlib.h>
#include <msgpack/unpack.h>
#include <msgpack/object.h>
#include "rpc/sb-rpc.h"
#include "helper-unix.h"
#include "helper-all.h"

bool wrap_crypto_write = true;

int __real_crypto_write(struct crypto_context *cc, char *data,
    size_t length, outputstream *out);

int __wrap_crypto_write(struct crypto_context *cc, char *buffer,
    size_t len, outputstream *ostream)
{
  if (!wrap_crypto_write) {
    return __real_crypto_write(cc, buffer, len, ostream);
  }

  msgpack_object deserialized;
  msgpack_zone mempool;
  msgpack_zone_init(&mempool, 2048);

  msgpack_unpack(buffer, len, NULL, &mempool, &deserialized);
  check_expected(&deserialized);
  msgpack_zone_destroy(&mempool);

  return (0);
}

int __wrap_outputstream_write(UNUSED(outputstream *ostream), char *buffer, size_t len)
{
  /* check if first 7 byte of packet identifier are correct */
  if (!(byte_isequal(buffer, 7, "rZQTd2n")))
    return (-1);

  switch(buffer[7]) {
  case 'T':
    /* tunnel packet */
    assert_int_equal(0, validate_crypto_tunnel(buffer, len));
    break;
  case 'M':
    /* message packet */
    assert_int_equal(0, validate_crypto_write(buffer, len));
    break;
  default:
    LOG_WARNING("Illegal identifier suffix.");
    return (-1);
  }

  return (0);
}

struct callinfo *__wrap_loop_wait_for_response(UNUSED(struct connection *con),
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
