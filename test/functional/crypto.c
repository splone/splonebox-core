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
#include "rpc/db/sb-db.h"
#include "tweetnacl.h"
#include "helper-unix.h"
#include "helper-all.h"

unsigned char clientshorttermpk[32];
unsigned char clientshorttermsk[32];
unsigned char clientlongtermpk[32];
unsigned char clientlongtermsk[32];
unsigned char serverlongtermpk[32];
unsigned char servershorttermpk[32];
unsigned char cookie[96];

struct crypto_context cc;

int validate_crypto_cookie_packet(unsigned char *buffer,
    UNUSED(uint64_t length))
{
  unsigned char *block;
  unsigned char *ciphertextpadded;
  unsigned char nonce[crypto_box_NONCEBYTES];
  uint64_t ciphertextlen;
  uint64_t blocklen;

  memcpy(nonce, "splonePK", 8);
  memcpy(nonce + 8, buffer + 8, 16);

  ciphertextlen = 160;
  blocklen = 144;

  block = malloc_array_or_die(ciphertextlen, sizeof(unsigned char));
  ciphertextpadded = calloc_or_die(ciphertextlen, sizeof(unsigned char));

  memcpy(ciphertextpadded + 16, buffer + 24, blocklen);

  if (crypto_box_open(block, ciphertextpadded, ciphertextlen, nonce,
      serverlongtermpk, clientshorttermsk)) {
    FREE(block);
    FREE(ciphertextpadded);
    return (-1);
  }

  memcpy(servershorttermpk, block + 32, 32);
  memcpy(cookie, block + 64, 96);

  FREE(block);
  FREE(ciphertextpadded);

  return (0);
}

int validate_crypto_write(unsigned char *buffer, UNUSED(uint64_t length))
{
  unsigned char *block;
  unsigned char *ciphertextpadded;
  unsigned char nonce[crypto_box_NONCEBYTES];
  unsigned char lengthpacked[40];
  unsigned char lengthbox[40] = {0};
  uint64_t unpackedlen;
  uint64_t packetnonce;
  uint64_t ciphertextlen;
  uint64_t blocklen;

  /* unpack nonce and check it's validity */
  packetnonce = uint64_unpack(buffer + 8);
  /* nonce is prefixed with 16-byte string "splonbox-client" */
  memcpy(nonce, "splonebox-server", 16);
  memcpy(nonce + 16, buffer + 8, 8);

  memcpy(lengthbox + 16, buffer + 16, 24);

  if (crypto_box_open_afternm(lengthpacked, lengthbox, 40, nonce,
      cc.clientshortservershort)) {
    return (-1);
  }

  unpackedlen = uint64_unpack(lengthpacked + 32);

  ciphertextlen = unpackedlen - 24;
  blocklen = unpackedlen - 40;

  block = malloc_array_or_die(ciphertextlen, sizeof(unsigned char));
  ciphertextpadded = calloc_or_die(ciphertextlen, sizeof(unsigned char));

  memcpy(ciphertextpadded + 16, buffer + 40, blocklen);

  uint64_pack(nonce + 16, packetnonce + 2);

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
  unsigned char initiatenonce[crypto_box_NONCEBYTES];
  unsigned char hellopacket[192] = {0};
  unsigned char initiatepacket[256] = {0};
  unsigned char messagepacket[120];
  unsigned char messagepacketout[120] = {0};
  unsigned char allzeroboxed[96] = {0};
  unsigned char initiatebox[160] = {0};
  unsigned char pubkeybox[96] = {0};
  unsigned char lengthbox[40] = {0};
  uint64_t plaintextlen;
  uint64_t readlen;
  outputstream write;

  connect_to_db();

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

  /* pack hello packet */
  memcpy(hellopacket, "oqQN2kaH", 8);
  /* pack compressed nonce */
  memcpy(hellopacket + 104, nonce + 16, 8);

  /* generate client ephemeral keys */
  if (crypto_box_keypair(clientlongtermpk, clientlongtermsk) != 0)
    return;
  /* generate client ephemeral keys */
  if (crypto_box_keypair(clientshorttermpk, clientshorttermsk) != 0)
    return;

  memcpy(hellopacket + 8, clientshorttermpk, 32);

  assert_int_equal(0, crypto_box(allzeroboxed, allzeroboxed, 96, nonce,
      serverlongtermpk, clientshorttermsk));

  memcpy(hellopacket + 112, allzeroboxed + 16, 80);

  crypto_init();

  /* positiv test */
  assert_int_equal(0, crypto_recv_hello_send_cookie(&cc, hellopacket, &write));

  /* wrong identifier */
  memcpy(hellopacket, "deadbeef", 8);
  assert_int_not_equal(0, crypto_recv_hello_send_cookie(&cc, hellopacket, &write));
  memcpy(hellopacket, "oqQN2kaH", 8);

  /* wrong nonce */
  cc.receivednonce = cc.nonce + 1;
  assert_int_not_equal(0, crypto_recv_hello_send_cookie(&cc, hellopacket, &write));
  cc.receivednonce = 0;

  /* wrong pubkey */
  memset(hellopacket + 8, '0', 32);
  assert_int_not_equal(0, crypto_recv_hello_send_cookie(&cc, hellopacket, &write));
  memcpy(hellopacket + 8, clientshorttermpk, 32);

  assert_int_equal(0, crypto_recv_hello_send_cookie(&cc, hellopacket, &write));

  /* crypto_recv_initiate() test */

  /* pack initiate packet */
  memcpy(initiatepacket, "oqQN2kaI", 8);
  memcpy(initiatepacket + 8, cookie, 96);
  /* pack compressed nonce */
  memcpy(initiatepacket + 104, nonce + 16, 8);

  memcpy(initiatebox + 32, clientlongtermpk, 32);
  randombytes(initiatebox + 64, 16);
  memcpy(initiatenonce, "splonePV", 8);
  memcpy(initiatenonce + 8, initiatebox + 64, 16);

  memcpy(pubkeybox + 32, clientshorttermpk, 32);
  memcpy(pubkeybox + 64, servershorttermpk, 32);

  assert_int_equal(0, crypto_box(pubkeybox, pubkeybox, 96, initiatenonce,
      serverlongtermpk, clientlongtermsk));

  memcpy(initiatebox + 80, pubkeybox + 16, 80);

  assert_int_equal(0, crypto_box(initiatebox, initiatebox, 160, nonce,
      servershorttermpk, clientshorttermsk));

  memcpy(initiatepacket + 112, initiatebox + 16, 144);

  /* without valid certificate */
  assert_int_not_equal(0, crypto_recv_initiate(&cc, initiatepacket));

  /* all plugins are allowed to connect */
  db_authorized_set_whitelist_all();
  assert_int_equal(0, crypto_recv_initiate(&cc, initiatepacket));

  /* crypto_write() test */
  assert_int_equal(0, crypto_write(&cc, (char*) allzeroboxed,
      sizeof(allzeroboxed), &write));

  /* crypto_read() test */

  /* pack message packet */
  memcpy(messagepacket, "oqQN2kaM", 8);

  /* pack compressed nonce */
  memcpy(nonce, "splonebox-client", 16);
  uint64_pack(nonce + 16, cc.nonce);
  memcpy(messagepacket + 8, nonce + 16, 8);

  uint64_pack(lengthbox + 32, 120);

  assert_int_equal(0, crypto_box(lengthbox, lengthbox, 40, nonce,
      servershorttermpk, clientshorttermsk));

  memcpy(messagepacket + 16, lengthbox + 16, 24);

  uint64_pack(nonce + 16, cc.nonce + 2);

  memset(allzeroboxed, 0, 96);
  assert_int_equal(0, crypto_box_afternm(allzeroboxed, allzeroboxed, 96, nonce,
      cc.clientshortservershort));

  memcpy(messagepacket + 40, allzeroboxed + 16, 80);

  assert_int_equal(0, crypto_verify_header(&cc, messagepacket, &readlen));

  assert_int_equal(0, crypto_read(&cc, messagepacket, (char*)messagepacketout,
      readlen, &plaintextlen));

  db_close();
}
