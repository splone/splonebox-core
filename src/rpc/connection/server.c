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

#include "rpc/connection/server.h"
#ifdef __linux__
#include <bsd/string.h>           // for strlcpy
#endif
#include <netdb.h>                // for getnameinfo, NI_MAXHOST, NI_MAXSERV
#include <netinet/in.h>           // for sockaddr_in
#include <stddef.h>               // for NULL, size_t
#include <stdint.h>               // for uint16_t
#include <sys/socket.h>           // for sockaddr
#include "khash.h"                // for __i, khint32_t
#include "main.h"                 // for main_loop
#include "rpc/connection/loop.h"  // for loop
#include "rpc/db/sb-db.h"         // for db_authorized_set_whitelist_all
#include "rpc/sb-rpc.h"           // for hashmap_cstr_t_ptr_t, hashmap_cstr_...
#include "sb-common.h"            // for fmt_addr, ::SERVER_TYPE_TCP, LOG_ERROR

#define ADDRESS_MAX_SIZE 256
#define MAX_CONNECTIONS 16
#define NUL '\000'


struct server {
  server_type type;
  union {
    struct {
      uv_tcp_t handle;
      struct sockaddr addr;
    } tcp;
    struct {
      uv_pipe_t handle;
      char addr[ADDRESS_MAX_SIZE];
    } pipe;
  } socket;
};

static hashmap(cstr_t, ptr_t) *servers = NULL;

int server_init(void)
{
  servers = hashmap_new(cstr_t, ptr_t)();

  if (!servers)
    return (-1);

  /* for now, we allow all plugins to connect */
  if (0 > db_authorized_set_whitelist_all())
    return (-1);

  return (0);
}

int server_start_tcp(boxaddr *addr, uint16_t port)
{
  int result;
  struct server *server = NULL;

  sbassert(addr);

  server = MALLOC(struct server);

  if (hashmap_has(cstr_t, ptr_t)(servers, fmt_addr(addr))) {
    LOG("Already listening on %s", fmt_addr(addr));
    return (-1);
  }

  if (server == NULL)
    return (-1);

  uv_stream_t *stream = NULL;

  box_addr_to_sockaddr(addr, port, &server->socket.tcp.addr,
      sizeof(struct sockaddr_in));

  uv_tcp_init(&main_loop.uv, &server->socket.tcp.handle);
  result = uv_tcp_bind(&server->socket.tcp.handle,
      (const struct sockaddr *)&server->socket.tcp.addr, 0);

  if (result) {
    LOG_WARNING("Failed to bind Socket %s", fmt_addr(addr));
    return (-1);
  }

  stream = (uv_stream_t *)&server->socket.tcp.handle;
  result = uv_listen(stream, MAX_CONNECTIONS, connection_cb);

  if (result) {
    LOG_WARNING("Already listening on %s\n", fmt_addr(addr));
    return (-1);
  }

  server->type = SERVER_TYPE_TCP;
  stream->data = server;

  hashmap_put(cstr_t, ptr_t)(servers, fmt_addr(addr), server);

  LOG_VERBOSE(VERBOSE_LEVEL_0, "Listening on %s:%d..\n", fmt_addr(addr), port);
  return (0);
}


int server_start_pipe(char *name)
{
  int result;
  struct server *server = NULL;

  sbassert(name);

  server = MALLOC(struct server);

  if (hashmap_has(cstr_t, ptr_t)(servers, name)) {
    LOG("Already listening on %s", name);
    return (-1);
  }

  if (server == NULL)
    return (-1);

  uv_stream_t *stream = NULL;

  if (strlcpy(server->socket.pipe.addr, name, sizeof(server->socket.pipe.addr))
      >= sizeof(server->socket.pipe.addr)) {
    LOG_WARNING("Failed to store addr in socket.pipe.addr!");
    return (-1);
  }

  uv_pipe_init(&main_loop.uv, &server->socket.pipe.handle, 0);
  result = uv_pipe_bind(&server->socket.pipe.handle, server->socket.pipe.addr);

  if (result) {
    LOG_WARNING("Binding the pipe \"%s\" to a file path failed", name);
    return (-1);
  }

  stream = (uv_stream_t *)&server->socket.pipe.handle;
  result = uv_listen(stream, MAX_CONNECTIONS, connection_cb);

  if (result) {
    LOG("Already listening on %s\n", name);
    return (-1);
  }

  server->type = SERVER_TYPE_PIPE;
  stream->data = server;

  hashmap_put(cstr_t, ptr_t)(servers, name, server);

  return (0);
}


int server_close(void)
{
  struct server *server;

  hashmap_foreach_value(servers, server, {
    if (server->type == SERVER_TYPE_TCP)
      uv_close((uv_handle_t *)&server->socket.tcp.handle, server_free_cb);
    else
      uv_close((uv_handle_t *)&server->socket.pipe.handle, server_free_cb);
  });

  hashmap_free(cstr_t, ptr_t)(servers);

  return (0);
}

int server_stop(char * endpoint)
{
  struct server *server;

  server = hashmap_get(cstr_t, ptr_t)(servers, endpoint);

  if (server->type == SERVER_TYPE_TCP)
    uv_close((uv_handle_t *)&server->socket.tcp.handle, server_free_cb);
  else
    uv_close((uv_handle_t *)&server->socket.pipe.handle, server_free_cb);

  hashmap_del(cstr_t, ptr_t)(servers, endpoint);

  return (0);
}

STATIC void connection_cb(uv_stream_t *server_stream, int status)
{
  int result;
  int namelen;
  uv_stream_t *client;
  struct server *server;
  struct sockaddr sockname;
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  size_t hbuflen;

  if (status == -1) {
    LOG_ERROR("error on_write_end");
    return;
  }

  namelen = sizeof(sockname);
  hbuflen = sizeof(hbuf);

  server = server_stream->data;
  client = MALLOC(uv_stream_t);

  if (client == NULL)
    return;

  if (server->type == SERVER_TYPE_TCP)
    uv_tcp_init(&main_loop.uv, (uv_tcp_t *)client);
  else
    uv_pipe_init(&main_loop.uv, (uv_pipe_t *)client, 0);

  result = uv_accept(server_stream, client);

  if (result) {
    LOG_ERROR("Failed to accept connection: %s", uv_strerror(result));
    uv_close((uv_handle_t *)client, client_free_cb);
    return;
  }

  if (server->type == SERVER_TYPE_TCP) {
    if (uv_tcp_getsockname((uv_tcp_t *)client, &sockname, &namelen) != 0) {
      uv_close((uv_handle_t *)client, client_free_cb);
      return;
    }

    if (getnameinfo(&sockname, sizeof(sockname), hbuf, sizeof(hbuf), sbuf,
        sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
      LOG_ERROR("could not get numeric hostname");
      uv_close((uv_handle_t *)client, client_free_cb);
      return;
    }
    LOG_VERBOSE(VERBOSE_LEVEL_0, "new client connection: host = %s, port = %s\n",
        hbuf, sbuf);
  } else {
    if (uv_pipe_getsockname((uv_pipe_t *)client, hbuf, &hbuflen) != 0) {
      uv_close((uv_handle_t *)client, client_free_cb);
      return;
    }
    LOG_VERBOSE(VERBOSE_LEVEL_0, "new client connection: host = %s\n", hbuf);
  }

  if (connection_create(client) < 0) {
    LOG_ERROR("Failed to create connection.");
    uv_close((uv_handle_t *)client, client_free_cb);
  }
}


STATIC void client_free_cb(uv_handle_t *handle)
{
  FREE(handle);
}


STATIC void server_free_cb(uv_handle_t *handle)
{
  struct server *server = (struct server*) handle->data;

  FREE(server);
}
