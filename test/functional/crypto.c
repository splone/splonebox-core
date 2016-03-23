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

#include "sb-common.h"
#include "rpc/sb-rpc.h"
#include "tweetnacl.h"
#include "helper-unix.h"

unsigned char clientshorttermpk[32];
unsigned char clientshorttermsk[32];
unsigned char serverlongtermpk[32];

struct crypto_context cc;

int validate_crypto_tunnel(unsigned char *buffer, uint64_t length)
{
  unsigned char *block;
  unsigned char *ciphertextpadded;
  unsigned char nonce[crypto_box_NONCEBYTES];
  uint64_t packetnonce;
  uint64_t ciphertextlen;
  uint64_t blocklen;
  /* read length */
  length = uint64_unpack(buffer + 8);
  /* unpack nonce and check it's validity */
  packetnonce = uint64_unpack(buffer + 16);
  /* nonce is prefixed with 16-byte string "splonbox-client" */
  memcpy(nonce, "splonebox-server", 16);
  memcpy(nonce + 16, buffer + 16, 8);

  ciphertextlen = length - 8;
  blocklen = length - 24;

  block = MALLOC_ARRAY(ciphertextlen, unsigned char);
  ciphertextpadded = CALLOC(ciphertextlen, unsigned char);

  if (block == NULL || ciphertextpadded == NULL)
    return (-1);

  memcpy(ciphertextpadded + 16, buffer + 24, blocklen);

  if (crypto_box_open(block, ciphertextpadded, ciphertextlen, nonce,
      serverlongtermpk, clientshorttermsk)) {
    FREE(block);
    FREE(ciphertextpadded);
    return (-1);
  }

  FREE(block);
  FREE(ciphertextpadded);

  return (0);
}

int validate_crypto_write(unsigned char *buffer, uint64_t length)
{
  unsigned char *block;
  unsigned char *ciphertextpadded;
  unsigned char nonce[crypto_box_NONCEBYTES];
  uint64_t packetnonce;
  uint64_t ciphertextlen;
  uint64_t blocklen;
  /* read length */
  length = uint64_unpack(buffer + 8);
  /* unpack nonce and check it's validity */
  packetnonce = uint64_unpack(buffer + 16);
  /* nonce is prefixed with 16-byte string "splonbox-client" */
  memcpy(nonce, "splonebox-server", 16);
  memcpy(nonce + 16, buffer + 16, 8);

  ciphertextlen = length - 8;
  blocklen = length - 24;

  block = MALLOC_ARRAY(ciphertextlen, unsigned char);
  ciphertextpadded = CALLOC(ciphertextlen, unsigned char);

  if (block == NULL || ciphertextpadded == NULL)
    return (-1);

  memcpy(ciphertextpadded + 16, buffer + 24, blocklen);

  if (crypto_box_open_afternm(block, ciphertextpadded, ciphertextlen, nonce,
      cc.clientshortservershort)) {
    FREE(block);
    FREE(ciphertextpadded);
    return (-1);
  }

  FREE(block);
  FREE(ciphertextpadded);

  return (0);
}

void functional_crypto(UNUSED(void **state))
{
  unsigned char nonce[crypto_box_NONCEBYTES];
  unsigned char tunnelpacket[136];
  unsigned char messagepacket[104];
  unsigned char messagepacketout[104] = {0};
  unsigned char allzeroboxed[96] = {0};
  uint64_t plaintextlen;
  outputstream write;

  wrap_crypto_write = false;

  assert_int_equal(0, filesystem_load(".keys/server-long-term.pub",
      serverlongtermpk, sizeof serverlongtermpk));

  cc.nonce = (uint64_t) randommod(281474976710656LL);

  if (!ISODD(cc.nonce)) {
    cc.nonce++;
  }

  cc.receivednonce = 0;
  cc.state = TUNNEL_INITIAL;

  memcpy(nonce, "splonebox-client", 16);
  uint64_pack(nonce + 16, cc.nonce);

  /* pack tunnel packet */
  memcpy(tunnelpacket, "oqQN2kaT", 8);
  /* pack length (8 id + 8 compressed nonce + 8 length + 48 server pub key) */
  uint64_pack(tunnelpacket + 8, 136);
  /* pack compressed nonce */
  memcpy(tunnelpacket + 16, nonce + 16, 8);

  /* generate client ephemeral keys */
  if (crypto_box_keypair(clientshorttermpk, clientshorttermsk) != 0)
    return;

  memcpy(tunnelpacket + 24, clientshorttermpk, 32);

  assert_int_equal(0, crypto_box(allzeroboxed, allzeroboxed, 96, nonce,
      serverlongtermpk, clientshorttermsk));

  memcpy(tunnelpacket + 56, allzeroboxed + 16, 80);

  crypto_init();

  /* positiv test */
  assert_int_equal(0, crypto_tunnel(&cc, tunnelpacket, &write));

  /* wrong identifier */
  memcpy(tunnelpacket, "deadbeef", 8);
  assert_int_not_equal(0, crypto_tunnel(&cc, tunnelpacket, &write));
  memcpy(tunnelpacket, "oqQN2kaT", 8);

  /* wrong nonce */
  cc.receivednonce = cc.nonce + 1;
  assert_int_not_equal(0, crypto_tunnel(&cc, tunnelpacket, &write));
  cc.receivednonce = 0;

  /* wrong pubkey */
  memset(tunnelpacket + 24, '0', 32);
  assert_int_not_equal(0, crypto_tunnel(&cc, tunnelpacket, &write));

  /* crypto_write() test */
  assert_int_equal(0, crypto_write(&cc, (char*) allzeroboxed,
      sizeof(allzeroboxed), &write));

  /* crypto_read() test */

  /* pack message packet */
  memcpy(messagepacket, "oqQN2kaM", 8);
  uint64_pack(messagepacket + 8, 104);
  /* pack compressed nonce */
  memcpy(nonce, "splonebox-client", 16);
  uint64_pack(nonce + 16, cc.nonce);
  memcpy(messagepacket + 16, nonce + 16, 8);

  memset(allzeroboxed, 0, 96);
  assert_int_equal(0, crypto_box_afternm(allzeroboxed, allzeroboxed, 96, nonce,
      cc.clientshortservershort));

  memcpy(messagepacket + 24, allzeroboxed + 16, 80);

  assert_int_equal(0, crypto_read(&cc, messagepacket, (char*)messagepacketout,
      104, &plaintextlen));
}
