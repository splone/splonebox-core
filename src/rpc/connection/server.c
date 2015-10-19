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

#include <stddef.h>
#include <string.h>
#ifdef __linux__
#include <bsd/string.h>
#endif
#include <stdlib.h>

#include "sb-common.h"
#include "rpc/sb-rpc.h"


#define ADDRESS_MAX_SIZE 256
#define MAX_CONNECTIONS 16
#define NUL '\000'

typedef enum {
  SERVER_TYPE_TCP,
  SERVER_TYPE_PIPE,
  SERVER_TYPE_UNKNOWN
} server_type;

struct server {
  server_type type;
  union {
    struct {
      uv_tcp_t handle;
      struct sockaddr_in addr;
    } tcp;
    struct {
      uv_pipe_t handle;
      char addr[ADDRESS_MAX_SIZE];
    } pipe;
  } socket;
};

static server_type type = SERVER_TYPE_TCP;

static struct hashmap *servers = NULL;

static server_type server_get_endpoint_type(string endpoint,
    struct server *server);
static void connection_cb(uv_stream_t *server_stream, int status);
static void client_free_cb(uv_handle_t *handle);
static void server_free_cb(uv_handle_t *handle);

int server_init(void)
{
  servers = hashmap_new();

  if (!servers)
    return (-1);

  return (0);
}


int server_start(string endpoint)
{
  int result;
  struct server *server = NULL;

  if (endpoint.length >= ADDRESS_MAX_SIZE) {
    LOG("endpoint too large");
    return (-1);
  }

  if (hashmap_contains_key(servers, endpoint)) {
    LOG("Already listening on %s", endpoint.str);
    return (-1);
  }

  server = MALLOC(struct server);

  if (server == NULL)
    return (-1);

  type = server_get_endpoint_type(endpoint, server);
  uv_stream_t *stream = NULL;

  if (type == SERVER_TYPE_TCP) {
    uv_tcp_init(uv_default_loop(), &server->socket.tcp.handle);
    result = uv_tcp_bind(&server->socket.tcp.handle,
        (const struct sockaddr *)&server->socket.tcp.addr, 0);

    if (result) {
      LOG("Failed to bind Socket %s", endpoint.str);
      return (-1);
    }

    stream = (uv_stream_t *)&server->socket.tcp.handle;
  } else {
    if (strlcpy(server->socket.pipe.addr, endpoint.str,
        sizeof(server->socket.pipe.addr)) >= sizeof(server->socket.pipe.addr)) {
      LOG("Failed to store addr in socket.pipe.addr!");
      return (-1);
    }

    uv_pipe_init(uv_default_loop(), &server->socket.pipe.handle, 0);
    result =
        uv_pipe_bind(&server->socket.pipe.handle, server->socket.pipe.addr);

    if (result) {
      LOG("Binding the pipe \"%s\" to a file path failed", endpoint.str);
      return (-1);
    }

    stream = (uv_stream_t *)&server->socket.pipe.handle;
  }

  result = uv_listen(stream, MAX_CONNECTIONS, connection_cb);

  if (result) {
    LOG("Already listening on %s\n", endpoint.str);
    return (-1);
  }

  server->type = type;
  stream->data = server;

  hashmap_put(servers, endpoint, server);

  return (0);
}


int server_close(void)
{
  struct server *server;

  HASHMAP_ITERATE_VALUE(servers, server, {
    if (server->type == SERVER_TYPE_TCP)
      uv_close((uv_handle_t *)&server->socket.tcp.handle, server_free_cb);
    else
      uv_close((uv_handle_t *)&server->socket.pipe.handle, server_free_cb);
  });

  hashmap_free(servers);

  return (0);
}

int server_stop(string endpoint)
{
  struct server *server;

  server = hashmap_get(servers, endpoint);

  if (server->type == SERVER_TYPE_TCP)
    uv_close((uv_handle_t *)&server->socket.tcp.handle, server_free_cb);
  else
    uv_close((uv_handle_t *)&server->socket.pipe.handle, server_free_cb);

  hashmap_remove(servers, endpoint);

  return (0);
}


static server_type server_get_endpoint_type(string endpoint,
    struct server *server)
{
  char ip[16] = "";
  char *endpoint_colon_char;
  int port;
  long lport;
  size_t ip_addr_len;

  type = SERVER_TYPE_TCP;
  port = -1;

  /* ip address and port handling */
  endpoint_colon_char = strrchr(endpoint.str, ':');

  if (!endpoint_colon_char)
    endpoint_colon_char = strchr(endpoint.str, NUL);

  ip_addr_len = (size_t)(endpoint_colon_char - endpoint.str);

  if (ip_addr_len > sizeof(ip) - 1)
    ip_addr_len = sizeof(ip) - 1;

  if (*endpoint_colon_char == ':') {
    lport = strtol(endpoint_colon_char + 1, NULL, 10);

    if ((lport <= 0) || (lport > 0xffff)) {
      /*
       * port is not in a valid range, 0-65535. so we treat it as a named pipe
       * a unix socket
       */
      type = SERVER_TYPE_PIPE;
    } else if (strlcpy(ip, endpoint.str, ip_addr_len + 1) >= sizeof(ip))
      type = SERVER_TYPE_PIPE;

    port = (int)lport;
  }

  if (type == SERVER_TYPE_TCP)
    if (uv_ip4_addr(ip, port, &server->socket.tcp.addr)) {
      /* ip is not valid, so we treat it as a named pipe or a unix socket */
      type = SERVER_TYPE_PIPE;
    }

  return type;
}

static void connection_cb(uv_stream_t *server_stream, int status)
{
  int result;
  int namelen;
  uv_stream_t *client;
  struct server *server;
  struct sockaddr sockname;
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  size_t hbuflen;

  if (status == -1) {
    LOG("error on_write_end");
    return;
  }

  namelen = sizeof(sockname);
  hbuflen = sizeof(hbuf);

  server = server_stream->data;
  client = MALLOC(uv_stream_t);

  if (client == NULL)
    return;

  if (server->type == SERVER_TYPE_TCP)
    uv_tcp_init(uv_default_loop(), (uv_tcp_t *)client);
  else
    uv_pipe_init(uv_default_loop(), (uv_pipe_t *)client, 0);

  result = uv_accept(server_stream, client);

  if (result) {
    LOG("Failed to accept connection: %s", uv_strerror(result));
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

  connection_create(client);
}


static void client_free_cb(uv_handle_t *handle)
{
  FREE(handle);
}


static void server_free_cb(uv_handle_t *handle)
{
  struct server *server = (struct server*) handle->data;

  FREE(server);
}
