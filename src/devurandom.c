/**
 *    Copyright (C) 2016 splone UG
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static int fd = -1;

void randombytes(unsigned char *x, unsigned long long xlen)
{
  size_t nbytes;
  ssize_t bytes_read;

  if (fd == -1) {
    for (;;) {
      fd = open("/dev/urandom", O_RDONLY);
      if (fd != -1)
        break;
      sleep(1);
    }
  }

  while (xlen > 0) {
    if (xlen < 1048576)
      nbytes = xlen;
    else
      nbytes = 1048576;

    bytes_read = read(fd, x, nbytes);

    if (bytes_read < 1) {
      sleep(1);
      continue;
    }

    x += bytes_read;
    xlen -= (unsigned long long) bytes_read;
  }
}
