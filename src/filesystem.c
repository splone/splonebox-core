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

#include <poll.h>
#include <unistd.h>
#include "sb-common.h"

int filesystem_open_write(const char *fn)
{
  int fd;
#ifdef O_CLOEXEC
  fd = open(fn, O_CREAT | O_WRONLY | O_NONBLOCK | O_CLOEXEC, 0600);
  return (fd);
#else
  fd = open(fn, O_CREAT | O_WRONLY | O_NONBLOCK, 0600);

  if (fd == -1)
    return (-1);

  fcntl(fd, F_SETFD, 1);

  return (fd);
#endif
}

int filesystem_open_read(const char *fn)
{
  int fd;

#ifdef O_CLOEXEC
  fd = open(fn, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  return (fd);
#else
  fd = open(fn, O_RDONLY | O_NONBLOCK);

  if (fd == -1)
    return (-1);

  fcntl(fd, F_SETFD, 1);

  return (fd);
#endif
}

int filesystem_write_all(int fd, const void *x, size_t xlen)
{
  size_t w;
  ssize_t res;
  struct pollfd p;

  while (xlen > 0) {
    w = xlen;

    /* 2^20 chunk maximum */
    if (w > 1048576)
      w = 1048576;

    res = write(fd, x, w);

    if (res < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
	      p.fd = fd;
	      p.events = POLLOUT | POLLERR;
        poll(&p, 1, -1);

        continue;
      }
      return (-1);
    }

    x = (const char*)x + res;
    xlen -= (size_t)res;
  }

  return 0;
}

int filesystem_read_all(int fd, void *x, size_t xlen)
{
  size_t r;
  ssize_t res;

  while (xlen > 0) {
    r = xlen;

    if (r > 1048576)
      r = 1048576;

    res = read(fd, x, r);

    if (res == 0) errno = EPROTO;

    if (res <= 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
        continue;

      return (-1);
    }

    x = (char*)x + res;
    xlen -= (size_t)res;
  }
  return (0);
}

int filesystem_save_sync(const char *fn, const void *x, size_t xlen)
{
  int fd;
  int r;

  fd = filesystem_open_write(fn);

  if (fd == -1)
    return (-1);

  if (filesystem_write_all(fd, x, xlen) == -1)
    return (-1);

  r = fsync(fd);

  close(fd);

  return r;
}

int filesystem_load(const char *fn, void *x, size_t xlen)
{
  int fd;
  int r;

  fd = filesystem_open_read(fn);

  if (fd == -1)
    return (-1);

  r = filesystem_read_all(fd, x, xlen);

  close(fd);

  return (r);
}
