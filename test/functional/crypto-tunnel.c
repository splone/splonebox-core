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

void functional_crypto_tunnel(UNUSED(void **state))
{
  struct crypto_context cc;
  unsigned char nonce[crypto_box_NONCEBYTES];
  unsigned char packet[136];
  unsigned char allzeroboxed[96] = {0};
  outputstream write;

  if (filesystem_load(".keys/server-long-term.pub", serverlongtermpk,
      sizeof serverlongtermpk) == -1) {
    return;
  }

  cc.nonce = (uint64_t) randommod(281474976710656LL);
  cc.receivednonce = 0;
  cc.state = TUNNEL_INITIAL;

  memcpy(nonce, "splonebox-client", 16);
  uint64_pack(nonce + 16, cc.nonce);

  /* pack tunnel packet */
  memcpy(packet, "oqQN2kaT", 8);
  /* pack length (8 id + 8 compressed nonce + 8 length + 48 server pub key) */
  uint64_pack(packet + 8, 72);
  /* pack compressed nonce */
  memcpy(packet + 16, nonce + 16, 8);

  /* generate client ephemeral keys */
  if (crypto_box_keypair(clientshorttermpk, clientshorttermsk) != 0)
    return;

  memcpy(packet + 24, clientshorttermpk, 32);

  crypto_box(allzeroboxed, allzeroboxed, 96, nonce, serverlongtermpk,
      clientshorttermsk);

  memcpy(packet + 56, allzeroboxed + 16, 80);

  crypto_init();

  /* positiv test */
  assert_int_equal(0, crypto_tunnel(&cc, packet, &write));

  /* wrong identifier */
  memcpy(packet, "deadbeef", 8);
  assert_int_not_equal(0, crypto_tunnel(&cc, packet, &write));
  memcpy(packet, "oqQN2kaT", 8);

  /* wrong nonce */
  cc.receivednonce = cc.nonce + 1;
  assert_int_not_equal(0, crypto_tunnel(&cc, packet, &write));
  cc.receivednonce = 0;

  /* wrong pubkey */
  memset(packet + 24, '0', 32);
  assert_int_not_equal(0, crypto_tunnel(&cc, packet, &write));

}
