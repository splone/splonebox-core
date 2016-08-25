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
#include <unistd.h>

#include "sb-common.h"
#include "rpc/sb-rpc.h"

int8_t verbose_level = 0;

static void print_usage(const char *name)
{
  LOG("Usage: %s FILE\n\n"
      "Returns the plugin key given a plugin's public long-term key.\n",
      name);
}

int main(int argc, char **argv)
{
  unsigned char clientlongtermpk[CLIENTLONGTERMPK_ARRAY_SIZE];
  char *pluginkey = NULL;
  int fd;

  if (argc <= 1) {
    print_usage(argv[0]);
    return (1);
  }

  fd = filesystem_open_read(argv[1]);
  if (!fd)
    LOG_ERROR("Failed to open file.\n");

  if (0 > filesystem_read_all(fd, clientlongtermpk,
    CLIENTLONGTERMPK_ARRAY_SIZE)) {
    LOG("Failed to read enough bytes from key file. Maybe it is not a key file?\n");
    goto fail;
  }

  pluginkey = MALLOC_ARRAY((PLUGINKEY_SIZE*2) + 1, char);
  if (!pluginkey) {
    LOG("Failed to alloc mem for pluginkey_base16_encoded.\n");
    goto fail;
  }

  base16_encode(pluginkey, (PLUGINKEY_SIZE*2) + 1,
    (char *)&clientlongtermpk[24], PLUGINKEY_SIZE);
  LOG("%s\n", pluginkey);

fail:
  if (pluginkey)
    FREE(pluginkey);

  if (0 > close(fd))
    LOG_ERROR("Failed to close file.\n");

  return (0);
}
