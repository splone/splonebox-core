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
#include "tweetnacl.h"

int8_t verbose_level;

unsigned char pk[crypto_box_PUBLICKEYBYTES];
unsigned char sk[crypto_box_SECRETKEYBYTES];

static void print_usage(const char *name)
{
  LOG("Usage: %s [add]\n\n"
      "Manages plugins.\n\n"
      "Commands:\n"
      "  add  add a new API key to database\n",
      name);
}

int main(int argc, char **argv)
{
  if (!argv[0]) {
    print_usage(argv[0]);
    return (0);
  }

  umask(022);

  if (mkdir(".keys",0700) == -1)
    LOG_ERROR("unable to create .keys directory\n");

  /* generate server ephemeral keys */
  if (crypto_box_keypair(pk, sk) != 0) {
    LOG_ERROR("unable to create keypair\n");
  }

  if (filesystem_save_sync(".keys/server-long-term.pub", pk, sizeof(pk))) {
    LOG_ERROR("unable to create key file .keys/server-long-term.pub\n");
  }

  umask(077);

  if (filesystem_save_sync(".keys/server-long-term", sk, sizeof(sk))) {
    LOG_ERROR("unable to create key file .keys/server-long-term.pub\n");
  }

  return (0);
}
