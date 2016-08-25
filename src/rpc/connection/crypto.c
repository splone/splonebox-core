#include <stdint.h>
#include <unistd.h>
#include "sb-common.h"
#include "rpc/sb-rpc.h"
#include "rpc/db/sb-db.h"
#include "rpc/connection/crypto.h"
#include "tweetnacl.h"

#define CRYPTO_PREFIX_SPLONEBOXCLIENT	"splonebox-client"
#define CRYPTO_PREFIX_SPLONEBOXSERVER	"splonebox-server"
#define CRYPTO_PREFIX_KNONCE "splonePK"
#define CRYPTO_PREFIX_VNONCE "splonePV"

#define CRYPTO_ID_MESSAGE_SERVER "rZQTd2nM"
#define CRYPTO_ID_MESSAGE_CLIENT "oqQN2kaM"
#define CRYPTO_ID_COOKIE_SERVER	"rZQTd2nC"
#define CRYPTO_ID_HELLO_CLIENT "oqQN2kaH"
#define CRYPTO_ID_INITIATE_CLIENT	"oqQN2kaI"

#define CRYPTO_MINUTE_KEY	"minute-k"

static unsigned char serverlongtermsk[32];

static uint64_t counterlow;
static uint64_t counterhigh;
static unsigned char flagkeyloaded;
static unsigned char noncekey[32];

STATIC int crypto_block(unsigned char *out, const unsigned char *in,
    const unsigned char *k);
STATIC int safenonce(unsigned char *y, int flaglongterm);
STATIC void nonce_update(struct crypto_context *cc);

int crypto_init(void)
{
  if (filesystem_load(".keys/server-long-term", serverlongtermsk,
      sizeof serverlongtermsk) == -1)
    return -1;

  return 0;
}


STATIC int safenonce(unsigned char *y, int flaglongterm)
{
  unsigned char data[16];

  sbassert(y);

  if (!flagkeyloaded) {
    int fdlock;

    fdlock = filesystem_open_lock(".keys/lock");

    if (fdlock == -1)
      return -1;

    if (filesystem_load(".keys/noncekey", noncekey, sizeof noncekey) == -1) {
      close(fdlock);
      return -1;
    }

    if (close(fdlock) == -1)
      return -1;

    flagkeyloaded = 1;
  }

  if (counterlow >= counterhigh) {
    int fdlock;

    fdlock = filesystem_open_lock(".keys/lock");

    if (fdlock == -1)
      return -1;

    if (filesystem_load(".keys/noncecounter", data, 8) == -1) {
      close(fdlock);
      return -1;
    }

    counterlow = uint64_unpack(data);

    if (flaglongterm)
      counterhigh = counterlow + 1048576;
    else
      counterhigh = counterlow + 1;

    uint64_pack(data, counterhigh);

    if (filesystem_save_sync(".keys/noncecounter", data, 8) == -1) {
      close(fdlock);
      return -1;
    }

    if (close(fdlock) == -1)
      return -1;
  }

  randombytes(data + 8, 8);
  uint64_pack(data, counterlow++);
  crypto_block(y, data, noncekey);

  return 0;
}

STATIC int crypto_block(unsigned char *out, const unsigned char *in,
    const unsigned char *k)
{
  int i;

  sbassert(out);
  sbassert(in);
  sbassert(k);

  uint64_t v0 = uint64_unpack(in + 0);
  uint64_t v1 = uint64_unpack(in + 8);
  uint64_t k0 = uint64_unpack(k + 0);
  uint64_t k1 = uint64_unpack(k + 8);
  uint64_t k2 = uint64_unpack(k + 16);
  uint64_t k3 = uint64_unpack(k + 24);
  uint64_t sum = 0;
  uint64_t delta = 0x9e3779b97f4a7c15;

  for (i = 0; i < 32; ++i) {
    sum += delta;
    v0 += ((v1 << 7) + k0) ^ (v1 + sum) ^ ((v1 >> 12) + k1);
    v1 += ((v0 << 16) + k2) ^ (v0 + sum) ^ ((v0 >> 8) + k3);
  }

  uint64_pack(out + 0, v0);
  uint64_pack(out + 8, v1);

  return 0;
}


STATIC void nonce_update(struct crypto_context *cc)
{
  sbassert(cc);

  cc->nonce += 2;

  if (cc->nonce)
    return;

  /* very unlikely */
  LOG_ERROR("nonce space expired");
  exit(1);
}


int byte_isequal(const void *yv, long long ylen, const void *xv)
{
  sbassert(yv);
  sbassert(ylen);
  sbassert(xv);

  const unsigned char *y = yv;
  const unsigned char *x = xv;
  unsigned char diff = 0;

  while (ylen > 0) {
    diff |= (*y++ ^ *x++);
    --ylen;
  }

  return (256 - (unsigned int)diff) >> 8;
}


void uint64_pack(unsigned char *y, uint64_t x)
{
  sbassert(y);

  *y++ = (unsigned char)x;
  x >>= 8;
  *y++ = (unsigned char)x;
  x >>= 8;
  *y++ = (unsigned char)x;
  x >>= 8;
  *y++ = (unsigned char)x;
  x >>= 8;
  *y++ = (unsigned char)x;
  x >>= 8;
  *y++ = (unsigned char)x;
  x >>= 8;
  *y++ = (unsigned char)x;
  x >>= 8;
  *y++ = (unsigned char)x;
  x >>= 8;
}


uint64_t uint64_unpack(const unsigned char *x)
{
  uint64_t result;

  sbassert(x);

  result = x[7];
  result <<= 8;
  result |= x[6];
  result <<= 8;
  result |= x[5];
  result <<= 8;
  result |= x[4];
  result <<= 8;
  result |= x[3];
  result <<= 8;
  result |= x[2];
  result <<= 8;
  result |= x[1];
  result <<= 8;
  result |= x[0];

  return result;
}


void crypto_update_minutekey(struct crypto_context *cc)
{
  sbassert(cc);

  memcpy(cc->lastminutekey, cc->minutekey, sizeof cc->lastminutekey);
  randombytes(cc->minutekey, sizeof cc->minutekey);
}


int crypto_verify_header(struct crypto_context *cc, unsigned char *data,
    uint64_t *length)
{
  uint64_t packetnonce;
  unsigned char lengthpacked[40] = {0};
  unsigned char nonce[crypto_box_NONCEBYTES];

  sbassert(cc);
  sbassert(data);
  sbassert(length);

  if (!(byte_isequal(data, 8, CRYPTO_ID_MESSAGE_CLIENT)))
    return -1;

  /* unpack nonce and check it's validity */
  packetnonce = uint64_unpack(data + 8);

  if ((packetnonce <= cc->receivednonce) || !ISODD(packetnonce))
    return -1;

  /* nonce is prefixed with 16-byte string "splonbox-client" */
  memcpy(nonce, CRYPTO_PREFIX_SPLONEBOXCLIENT, 16);
  memcpy(nonce + 16, data + 8, 8);

  memcpy(lengthpacked + 16, data + 16, 24);

  if (crypto_box_open_afternm(lengthpacked, lengthpacked, 40, nonce,
      cc->clientshortservershort) != 0)
    return -1;

  cc->receivednonce = packetnonce;

  *length = uint64_unpack(lengthpacked + 32);

  if (*length < 40)
    return -1;

  return 0;
}


int crypto_recv_hello_send_cookie(struct crypto_context *cc,
    unsigned char *data, outputstream *out)
{
  unsigned char nonce[crypto_box_NONCEBYTES];
  unsigned char clientshortserverlong[32];
  unsigned char allzeroboxed[96] = { 0 };
  unsigned char cookiebox[160] = { 0 };
  unsigned char cookiepacket[168];
  uint64_t packetnonce;

  sbassert(cc);
  sbassert(data);
  sbassert(out);

  /* check if first 8 byte of packet identifier are correct */
  if (!(byte_isequal(data, 8, CRYPTO_ID_HELLO_CLIENT)))
    return -1;

  /* unpack nonce and check it's validity */
  packetnonce = uint64_unpack(data + 104);

  if ((packetnonce <= cc->receivednonce) || !ISODD(packetnonce))
    return -1;

  /* init clientshorttermpk */
  memcpy(cc->clientshorttermpk, data + 8, 32);

  crypto_box_beforenm(clientshortserverlong, cc->clientshorttermpk,
      serverlongtermsk);

  /* nonce is prefixed with 16-byte string "splonbox-client" */
  memcpy(nonce, CRYPTO_PREFIX_SPLONEBOXCLIENT, 16);
  memcpy(nonce + 16, data + 104, 8);

  memcpy(allzeroboxed + 16, data + 112, 80);

  /* check if box can be opened (authentication) */
  if (crypto_box_open_afternm(allzeroboxed, allzeroboxed, 96, nonce,
      clientshortserverlong))
    goto fail;

  /* send cookie packet */

  /* generate server ephemeral keys */
  if (crypto_box_keypair(cc->servershorttermpk, cc->servershorttermsk) != 0)
    goto fail;

  memcpy(cookiebox + 96, cc->clientshorttermpk, 32);
  memcpy(cookiebox + 128, cc->servershorttermsk, 32);

  memcpy(nonce, CRYPTO_MINUTE_KEY, 8);

  if (safenonce(nonce + 8, 1) == -1) {
    LOG_ERROR("nonce-generation disaster");
    goto fail;
  }

  if (crypto_secretbox(cookiebox + 64, cookiebox + 64, 96, nonce, cc->minutekey)
      != 0)
    goto fail;

  memcpy(cookiebox + 64, nonce + 8, 16);
  memcpy(cookiebox + 32, cc->servershorttermpk, 32);

  memcpy(nonce, CRYPTO_PREFIX_KNONCE, 8);

  if (crypto_box_afternm(cookiebox, cookiebox, 160, nonce,
      clientshortserverlong) != 0)
    goto fail;

  memcpy(cookiepacket, CRYPTO_ID_COOKIE_SERVER, 8);
  memcpy(cookiepacket + 8, nonce + 8, 16);
  memcpy(cookiepacket + 24, cookiebox + 16, 144);

  if (outputstream_write(out, (char *)cookiepacket, 168) < 0)
    goto fail;

  cc->state = TUNNEL_COOKIE_SENT;

  sbmemzero(clientshortserverlong, sizeof clientshortserverlong);
  sbmemzero(allzeroboxed, sizeof allzeroboxed);
  sbmemzero(cookiebox, sizeof cookiebox);
  sbmemzero(cookiepacket, sizeof cookiepacket);

  return 0;

fail:
  /* zero out sensitive data */
  sbmemzero(clientshortserverlong, sizeof clientshortserverlong);
  sbmemzero(cc->servershorttermpk, sizeof cc->servershorttermpk);
  sbmemzero(cc->clientshorttermpk, sizeof cc->clientshorttermpk);
  sbmemzero(allzeroboxed, sizeof allzeroboxed);
  sbmemzero(cookiebox, sizeof cookiebox);
  sbmemzero(cookiepacket, sizeof cookiepacket);

  return -1;
}


int crypto_recv_initiate(struct crypto_context *cc, unsigned char *data)
{
  unsigned char nonce[crypto_box_NONCEBYTES];
  unsigned char cookie[96] = { 0 };
  unsigned char servershorttermsk[32];
  unsigned char initiatebox[160] = { 0 };
  unsigned char clientlongtermpk[32];
  unsigned char clientlongserverlong[32];
  uint64_t packetnonce;

  sbassert(cc);
  sbassert(data);

  /* check if first 8 byte of packet identifier are correct */
  if (!(byte_isequal(data, 8, CRYPTO_ID_INITIATE_CLIENT)))
    return -1;

  memcpy(nonce, CRYPTO_MINUTE_KEY, 8);
  /* copy 16-byte cookie nonce */
  memcpy(nonce + 8, data + 8, 16);

  memcpy(cookie + 16, data + 24, 80);

  if (crypto_secretbox_open(cookie, cookie, 96, nonce, cc->minutekey)) {
    sbmemzero(cookie, 16);
    memcpy(cookie + 16, data + 24, 80);

    if (crypto_secretbox_open(cookie, cookie, 96, nonce, cc->lastminutekey))
      goto fail;
  }

  /* check if cookie C' is equal to the C' from the hello packet */
  if (!byte_isequal(cc->clientshorttermpk, 32, cookie + 32))
    goto fail;

  memcpy(servershorttermsk, cookie + 64, 32);

  /* use nacl shared secret precomputation interface */
  crypto_box_beforenm(cc->clientshortservershort, cc->clientshorttermpk,
      servershorttermsk);

  /* unpack nonce and check it's validity */
  packetnonce = uint64_unpack(data + 104);

  if ((packetnonce <= cc->receivednonce) || !ISODD(packetnonce))
    return -1;

  /* nonce is prefixed with 16-byte string "splonbox-client" */
  memcpy(nonce, CRYPTO_PREFIX_SPLONEBOXCLIENT, 16);
  memcpy(nonce + 16, data + 104, 8);

  memcpy(initiatebox + 16, data + 112, 144);

  if (crypto_box_open_afternm(initiatebox, initiatebox, 160, nonce,
      cc->clientshortservershort))
    goto fail;

  memcpy(clientlongtermpk, initiatebox + 32, 32);

  /* verify that the plugin is authorized to connecting */
  if (!db_authorized_whitelist_all_is_set() &&
      !db_authorized_verify(clientlongtermpk)) {
    LOG_VERBOSE(VERBOSE_LEVEL_0, "Failed to verify plugin long-term public key.\n");
    goto fail;
  }

  crypto_box_beforenm(clientlongserverlong, clientlongtermpk, serverlongtermsk);

  memcpy(nonce, CRYPTO_PREFIX_VNONCE, 8);
  memcpy(nonce + 8, initiatebox + 64, 16);

  sbmemzero(initiatebox + 64, 16);

  if (crypto_box_open_afternm(initiatebox + 64, initiatebox + 64, 96, nonce,
      clientlongserverlong))
    goto fail;

  if (!byte_isequal(initiatebox + 96, 32, cc->clientshorttermpk))
    goto fail;

  if (!byte_isequal(initiatebox + 128, 32, cc->servershorttermpk))
    goto fail;

  cc->receivednonce = packetnonce;

  cc->state = TUNNEL_ESTABLISHED;

  base16_encode(cc->pluginkeystring, PLUGINKEY_STRING_SIZE,
    (char *)&clientlongtermpk[24], PLUGINKEY_SIZE);

  sbmemzero(cookie, sizeof cookie);
  sbmemzero(servershorttermsk, sizeof servershorttermsk);
  sbmemzero(initiatebox, sizeof initiatebox);
  sbmemzero(clientlongtermpk, sizeof clientlongtermpk);
  sbmemzero(clientlongserverlong, sizeof clientlongserverlong);

  return 0;

fail:
  /* zero out sensitive data */
  sbmemzero(cookie, sizeof cookie);
  sbmemzero(servershorttermsk, sizeof servershorttermsk);
  sbmemzero(initiatebox, sizeof initiatebox);
  sbmemzero(clientlongtermpk, sizeof clientlongtermpk);
  sbmemzero(clientlongserverlong, sizeof clientlongserverlong);

  return -1;
}


int crypto_write(struct crypto_context *cc, char *data,
    size_t length, outputstream *out)
{
  unsigned long long packetlen;
  unsigned char *packet;
  unsigned char *block = NULL;
  unsigned char *ciphertext = NULL;
  unsigned char lengthbox[40] = { 0 };
  unsigned char nonce[crypto_box_NONCEBYTES];
  uint64_t blocklen;

  sbassert(cc);
  sbassert(data);
  sbassert(out);

  /*
   * add 8 byte for identifier, 24 byte for boxed length and 8 byte for
   * compressed nonce. nacl api also requires 32 byte zero-padding
   * (crypto_box_ZEROBYTES)
   */
  packetlen = length + 56;
  packet = MALLOC_ARRAY(packetlen, unsigned char);

  if (packet == NULL)
    return -1;

  /* update nonce */
  nonce_update(cc);

  memcpy(packet, CRYPTO_ID_MESSAGE_SERVER, 8);

  /* set nonce expansion prefix and compressed nonce (little-endian) */
  memcpy(nonce, CRYPTO_PREFIX_SPLONEBOXSERVER, 16);
  uint64_pack(nonce + 16, cc->nonce);

  /* pack compressed nonce */
  memcpy(packet + 8, nonce + 16, 8);

  uint64_pack(lengthbox + 32, packetlen);

  if (crypto_box_afternm(lengthbox, lengthbox, 40, nonce,
      cc->clientshortservershort) != 0)
    goto fail;

  /* pack boxed length */
  memcpy(packet + 16, lengthbox + 16, 24);

  blocklen = length + 32;
  block = CALLOC(blocklen, unsigned char);
  ciphertext = MALLOC_ARRAY(blocklen, unsigned char);

  if ((block == NULL) || (ciphertext == NULL))
    return -1;

  memcpy(block + 32, data, length);

  /* update nonce */
  nonce_update(cc);
  uint64_pack(nonce + 16, cc->nonce);

  if (crypto_box_afternm(ciphertext, block, blocklen, nonce,
      cc->clientshortservershort) != 0)
    goto fail;

  memcpy(packet + 40, ciphertext + 16, blocklen - 16);

  if (outputstream_write(out, (char *)packet, packetlen) < 0)
    goto fail;

  sbmemzero(packet, sizeof packet);
  sbmemzero(block, sizeof block);
  sbmemzero(ciphertext, sizeof ciphertext);
  FREE(ciphertext);
  FREE(block);
  FREE(packet);

  return 0;

fail:
  sbmemzero(packet, sizeof packet);
  sbmemzero(block, sizeof block);
  sbmemzero(ciphertext, sizeof ciphertext);
  FREE(ciphertext);
  FREE(block);
  FREE(packet);

  return -1;
}


int crypto_read(struct crypto_context *cc, unsigned char *in, char *out,
    uint64_t length, uint64_t *plaintextlen)
{
  unsigned char *block;
  unsigned char *ciphertextpadded;
  unsigned char nonce[crypto_box_NONCEBYTES];
  uint64_t ciphertextlen;
  uint64_t blocklen;

  sbassert(cc);
  sbassert(in);
  sbassert(out);
  sbassert(plaintextlen);

  /* nonce is prefixed with 16-byte string "splonbox-client" */

  memcpy(nonce, CRYPTO_PREFIX_SPLONEBOXCLIENT, 16);
  uint64_pack(nonce + 16, cc->receivednonce + 2);

  /* blocklen = length - 8 (id) - 24 (length) - 8 (nonce) */
  blocklen = length - 40;
  /* ciphertextlen = length - 8 (id) - 72 (length) - 8 (nonce) + 16 (padding) */
  ciphertextlen = length - 24;

  block = MALLOC_ARRAY(ciphertextlen, unsigned char);
  ciphertextpadded = CALLOC(ciphertextlen, unsigned char);

  if ((block == NULL) || (ciphertextpadded == NULL))
    return -1;

  memcpy(ciphertextpadded + 16, in + 40, blocklen);

  if (crypto_box_open_afternm(block, ciphertextpadded, ciphertextlen, nonce,
      cc->clientshortservershort) != 0)
    goto fail;

  *plaintextlen = length - 56;
  memcpy(out, block + 32, *plaintextlen);

  cc->receivednonce += 2;

  sbmemzero(block, sizeof block);
  sbmemzero(ciphertextpadded, sizeof ciphertextpadded);
  FREE(block);
  FREE(ciphertextpadded);

  return 0;

fail:
  sbmemzero(block, sizeof block);
  sbmemzero(ciphertextpadded, sizeof ciphertextpadded);
  FREE(block);
  FREE(ciphertextpadded);

  return -1;
}
