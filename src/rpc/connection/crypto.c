#include <stdint.h>
#include "sb-common.h"
#include "rpc/sb-rpc.h"
#include "tweetnacl.h"

/* server security */
static unsigned char serverlongtermsk[32];

int crypto_init(void)
{
  if (filesystem_load(".keys/server-long-term", serverlongtermsk,
      sizeof serverlongtermsk) == -1) {
    return (-1);
  }

  return (0);
}

static void nonce_update(struct crypto_context *cc)
{
  ++cc->nonce;
  if (cc->nonce)
    return;
  /* very unlikely */
  LOG_ERROR("nonce space expired");
  exit(1);
}

int byte_isequal(const void *yv, long long ylen, const void *xv)
{
  const unsigned char *y = yv;
  const unsigned char *x = xv;
  unsigned char diff = 0;

  while (ylen > 0) {
    diff |= (*y++ ^ *x++);
    --ylen;
  }

  return (256 - (unsigned int) diff) >> 8;
}

void uint64_pack(unsigned char *y, uint64_t x)
{
  *y++ = (unsigned char) x; x >>= 8;
  *y++ = (unsigned char) x; x >>= 8;
  *y++ = (unsigned char) x; x >>= 8;
  *y++ = (unsigned char) x; x >>= 8;
  *y++ = (unsigned char) x; x >>= 8;
  *y++ = (unsigned char) x; x >>= 8;
  *y++ = (unsigned char) x; x >>= 8;
  *y++ = (unsigned char) x; x >>= 8;
}

uint64_t uint64_unpack(const unsigned char *x)
{
  uint64_t result;

  result = x[7];
  result <<= 8; result |= x[6];
  result <<= 8; result |= x[5];
  result <<= 8; result |= x[4];
  result <<= 8; result |= x[3];
  result <<= 8; result |= x[2];
  result <<= 8; result |= x[1];
  result <<= 8; result |= x[0];

  return result;
}

int crypto_verify_header(unsigned char *data, uint64_t *length)
{
  if (!(byte_isequal(data, 8, "oqQN2kaM")))
    return (-1);

  *length = uint64_unpack(data + 8);

  return (0);
}

int crypto_tunnel(struct crypto_context *cc, unsigned char *data,
    outputstream *out)
{
  unsigned char servershorttermpk[32];
  unsigned char servershorttermsk[32];
  unsigned char clientshorttermpk[32];
  unsigned char clientshortserverlong[32];
  unsigned char nonce[crypto_box_NONCEBYTES];
  unsigned char packet[72];
  unsigned char allzeroboxed[96] = {0};
  unsigned char servershorttermpk0padded[64] = {0};
  unsigned char servershorttermpkboxed[64];
  uint64_t packetnonce;
  uint64_t length;

  /* check if first 8 byte of packet identifier are correct */
  if (!(byte_isequal(data, 8, "oqQN2kaT")))
    return (-1);

  /* unpack nonce and check it's validity */
  packetnonce = uint64_unpack(data + 16);

  if (packetnonce <= cc->receivednonce)
    return (-1);

  /* init clientshorttermpk */
  memcpy(clientshorttermpk, data + 24, 32);
  if (crypto_box_beforenm(clientshortserverlong, clientshorttermpk,
      serverlongtermsk) != 0) {
    goto fail;
  }

  /* read length */
  length = uint64_unpack(data + 8);
  /* nonce is prefixed with 16-byte string "splonbox-client" */
  memcpy(nonce, "splonebox-client", 16);
  memcpy(nonce + 16, data + 16, 8);
  /* copy box and add padding */
  memcpy(allzeroboxed + 16, data + 56,  80);

  /* check if box can be opened (authentication) */
  if (crypto_box_open_afternm(allzeroboxed, allzeroboxed, 96, nonce,
      clientshortserverlong)) {
    goto fail;
  }

  /* generate server ephemeral keys */
  if (crypto_box_keypair(servershorttermpk, servershorttermsk) != 0) {
    goto fail;
  }

  /* update nonce */
  nonce_update(cc);

  /* set nonce expansion prefix and compressed nonce (little-endian) */
  memcpy(nonce, "splonebox-server", 16);
  uint64_pack(nonce + 16, cc->nonce);

  /* boxing server short term public key */
  memcpy(servershorttermpk0padded + crypto_box_ZEROBYTES, servershorttermpk, 32);
  if (crypto_box_afternm(servershorttermpkboxed, servershorttermpk0padded, 64,
      nonce, clientshortserverlong) != 0) {
    goto fail;
  }

  /* pack tunnel packet */
  memcpy(packet, "rZQTd2nT", 8);
  /* pack length (8 id + 8 compressed nonce + 8 length + 48 server pub key) */
  uint64_pack(packet + 8, 72);
  /* pack compressed nonce */
  memcpy(packet + 16, nonce + 16, 8);
  /* pack boxed server public key without crypto_box_BOXZEROBYTES padding */
  memcpy(packet + 24, servershorttermpkboxed + 16, 48);

  if (outputstream_write(out, (char*)packet, 72) < 0) {
    goto fail;
  }

  /* use nacl shared secret precomputation interface */
  if (crypto_box_beforenm(cc->clientshortservershort, clientshorttermpk,
      servershorttermsk) != 0) {
    goto fail;
  }

  cc->state = TUNNEL_ESTABLISHED;

  sbmemzero(clientshortserverlong, sizeof clientshortserverlong);
  sbmemzero(servershorttermpk, sizeof servershorttermpk);
  sbmemzero(servershorttermsk, sizeof servershorttermsk);
  sbmemzero(clientshorttermpk, sizeof clientshorttermpk);
  sbmemzero(servershorttermpk0padded, sizeof servershorttermpk0padded);
  sbmemzero(servershorttermpkboxed, sizeof servershorttermpkboxed);
  sbmemzero(packet, sizeof packet);
  sbmemzero(allzeroboxed, sizeof allzeroboxed);
  sbmemzero(serverlongtermsk, sizeof serverlongtermsk);

  return (0);

fail:
  /* zero out sensitive data */
  sbmemzero(clientshortserverlong, sizeof clientshortserverlong);
  sbmemzero(servershorttermpk, sizeof servershorttermpk);
  sbmemzero(servershorttermsk, sizeof servershorttermsk);
  sbmemzero(clientshorttermpk, sizeof clientshorttermpk);
  sbmemzero(servershorttermpk0padded, sizeof servershorttermpk0padded);
  sbmemzero(servershorttermpkboxed, sizeof servershorttermpkboxed);
  sbmemzero(packet, sizeof packet);
  sbmemzero(allzeroboxed, sizeof allzeroboxed);

  return (-1);
}

int crypto_write(struct crypto_context *cc, char *data,
    size_t length, outputstream *out)
{
  unsigned long long packetlen;
  unsigned char *packet;
  unsigned char *block;
  unsigned char *ciphertext;
  unsigned char nonce[crypto_box_NONCEBYTES];
  uint64_t blocklen;

  /*
   * add 8 byte each for identifier, length and for compressed nonce. nacl api
   * also requires 32 byte zero-padding (crypto_box_ZEROBYTES)
   */
  packetlen = length + 40;
  packet = MALLOC_ARRAY(packetlen, unsigned char);

  if (packet == NULL)
    return (-1);

  /* update nonce */
  nonce_update(cc);

  memcpy(packet, "rZQTd2nM", 8);
  uint64_pack(packet + 8, packetlen);

  /* set nonce expansion prefix and compressed nonce (little-endian) */
  memcpy(nonce, "splonebox-server", 16);
  uint64_pack(nonce + 16, cc->nonce);
  /* pack compressed nonce */
  memcpy(packet + 16, nonce + 16, 8);

  blocklen = length + 32;
  block = CALLOC(blocklen, unsigned char);
  ciphertext = MALLOC_ARRAY(blocklen, unsigned char);

  memcpy(block + 32, data, length);

  if (crypto_box_afternm(ciphertext, block, blocklen, nonce,
      cc->clientshortservershort) != 0) {
    goto fail;
  }

  memcpy(packet + 24, ciphertext + 16, blocklen - 16);

  if (outputstream_write(out, (char*)packet, packetlen) < 0) {
    goto fail;
  }

  sbmemzero(packet, sizeof packet);
  sbmemzero(block, sizeof block);
  sbmemzero(ciphertext, sizeof ciphertext);
  FREE(ciphertext);
  FREE(block);
  FREE(packet);

  return (0);

fail:
  sbmemzero(packet, sizeof packet);
  sbmemzero(block, sizeof block);
  sbmemzero(ciphertext, sizeof ciphertext);
  FREE(ciphertext);
  FREE(block);
  FREE(packet);

  return (-1);
}

int crypto_read(struct crypto_context *cc, unsigned char *in, char *out,
    uint64_t length, uint64_t *plaintextlen)
{
  unsigned char *block;
  unsigned char *ciphertextpadded;
  unsigned char nonce[crypto_box_NONCEBYTES];
  uint64_t ciphertextlen;
  uint64_t blocklen;
  uint64_t packetnonce;

  if (cc == NULL || in == NULL || out == NULL || plaintextlen == NULL)
    return (-1);

  /* unpack nonce and check it's validity */
  packetnonce = uint64_unpack(in + 16);

  if (packetnonce <= cc->receivednonce)
    return (-1);

  /* nonce is prefixed with 16-byte string "splonbox-client" */
  memcpy(nonce, "splonebox-client", 16);
  memcpy(nonce + 16, in + 16, 8);

  /* blocklen = length - 8 (id) - 8 (length) - 8 (nonce) */
  blocklen = length - 24;
  /* ciphertextlen = length - 8 (id) - 8 (length) - 8 (nonce) + 16 (padding) */
  ciphertextlen = length - 8;

  block = MALLOC_ARRAY(ciphertextlen, unsigned char);
  ciphertextpadded = CALLOC(ciphertextlen, unsigned char);

  if (block == NULL || ciphertextpadded == NULL)
    return (-1);

  memcpy(ciphertextpadded + 16, in + 24, blocklen);

  if (crypto_box_open_afternm(block, ciphertextpadded, ciphertextlen, nonce,
      cc->clientshortservershort) != 0) {
    goto fail;
  }

  *plaintextlen = length - 40;
  memcpy(out, block + 32, *plaintextlen);

  cc->receivednonce = packetnonce;

  sbmemzero(block, sizeof block);
  sbmemzero(ciphertextpadded, sizeof ciphertextpadded);
  FREE(block);
  FREE(ciphertextpadded);

  return (0);

fail:
  sbmemzero(block, sizeof block);
  sbmemzero(ciphertextpadded, sizeof ciphertextpadded);
  FREE(block);
  FREE(ciphertextpadded);

  return (-1);
}
