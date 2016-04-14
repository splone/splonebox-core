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
 *
 *    This file incorporates code covered by the following terms:
 *
 *    Copyright (c) 2001-2004, Roger Dingledine
 *    Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson
 *    Copyright (c) 2007-2016, The Tor Project, Inc.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are
 *    met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following disclaimer
 *        in the documentation and/or other materials provided with the
 *        distribution.
 *
 *      * Neither the names of the copyright owners nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include "sb-common.h"
#include "address.h"

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wcast-align"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wcast-align"
#endif

/** Set <b>dest</b> to the IPv4 address incoded in <b>in</b>. */
#define box_addr_from_in(dest, in) \
  box_addr_from_ipv4n((dest), (in)->s_addr);

static inline uint32_t box_addr_to_ipv4n(const boxaddr *a);
static inline sa_family_t box_addr_family(const boxaddr *a);
static inline const struct in6_addr * box_addr_to_in6(const boxaddr *a);

/** Return an in6_addr* equivalent to <b>a</b>, or NULL if <b>a</b> is not
 * an IPv6 address. */
static inline const struct in6_addr * box_addr_to_in6(const boxaddr *a)
{
  return a->family == AF_INET6 ? &a->addr.in6_addr : NULL;
}

/** Return an IPv4 address in network order for <b>a</b>, or 0 if
 * <b>a</b> is not an IPv4 address. */
static inline uint32_t box_addr_to_ipv4n(const boxaddr *a)
{
  return a->family == AF_INET ? a->addr.in_addr.s_addr : 0;
}

/** Return the address family of <b>a</b>.  Possible values are:
 * AF_INET6, AF_INET, AF_UNSPEC. */
static inline sa_family_t box_addr_family(const boxaddr *a)
{
  return (a->family);
}

 /** Similar behavior to Unix gethostbyname: resolve <b>name</b>, and set
 * *<b>addr</b> to the proper IP address and family. The <b>family</b>
 * argument (which must be AF_INET, AF_INET6, or AF_UNSPEC) declares a
 * <i>preferred</i> family, though another one may be returned if only one
 * family is implemented for this address.
 *
 * Return 0 on success, -1 on failure; 1 on transient failure.
 */
int box_getaddrinfo(const char *name, uint16_t family, boxaddr *addr)
{
  struct in_addr iaddr;
  struct in6_addr iaddr6;

  assert(name);
  assert(addr);

  assert(family == AF_INET || family == AF_INET6 || family == AF_UNSPEC);

  if (!*name) {
    /* Empty address is an error. */
    return (-1);
  } else if (inet_pton(AF_INET, name, &iaddr)) {
    /* It's an IPv4 IP. */
    if (family == AF_INET6)
      return (-1);
    box_addr_from_in(addr, &iaddr);
    return (0);
  } else if (inet_pton(AF_INET6, name, &iaddr6)) {
    if (family == AF_INET)
      return (-1);
    box_addr_from_in6(addr, &iaddr6);
    return (0);
  } else {
#ifdef HAVE_GETADDRINFO
    int err;
    struct addrinfo *res=NULL, *res_p;
    struct addrinfo *best=NULL;
    struct addrinfo hints;
    int result = -1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;
    err = getaddrinfo(name, NULL, &hints, &res);
    /* The check for 'res' here shouldn't be necessary, but it makes static
     * analysis tools happy. */
    if (!err && res) {
      best = NULL;
      for (res_p = res; res_p; res_p = res_p->ai_next) {
        if (family == AF_UNSPEC) {
          if (res_p->ai_family == AF_INET) {
            best = res_p;
            break;
          } else if (res_p->ai_family == AF_INET6 && !best) {
            best = res_p;
          }
        } else if (family == res_p->ai_family) {
          best = res_p;
          break;
        }
      }
      if (!best)
        best = res;
      if (best->ai_family == AF_INET) {
        box_addr_from_in(addr, &((struct sockaddr_in*)best->ai_addr)->sin_addr);
        result = 0;
      } else if (best->ai_family == AF_INET6) {
        box_addr_from_in6(addr,
            &((struct sockaddr_in6*)best->ai_addr)->sin6_addr);
        result = 0;
      }
      freeaddrinfo(res);

      return (result);
    }
    return (err == EAI_AGAIN) ? 1 : -1;
#else
    struct hostent *ent;
    int err;
#ifdef HAVE_GETHOSTBYNAME_R_6_ARG
    char buf[2048];
    struct hostent hostent;
    int r;
    r = gethostbyname_r(name, &hostent, buf, sizeof(buf), &ent, &err);
#elif defined(HAVE_GETHOSTBYNAME_R_5_ARG)
    char buf[2048];
    struct hostent hostent;
    ent = gethostbyname_r(name, &hostent, buf, sizeof(buf), &err);
#elif defined(HAVE_GETHOSTBYNAME_R_3_ARG)
    struct hostent_data data;
    struct hostent hent;
    memset(&data, 0, sizeof(data));
    err = gethostbyname_r(name, &hent, &data);
    ent = err ? NULL : &hent;
#else
    ent = gethostbyname(name);
    err = h_errno;
#endif /* endif HAVE_GETHOSTBYNAME_R_6_ARG. */
    if (ent) {
      if (ent->h_addrtype == AF_INET) {
        box_addr_from_in(addr, (struct in_addr*)ent->h_addr);
      } else if (ent->h_addrtype == AF_INET6) {
        box_addr_from_in6(addr, (struct in6_addr*)ent->h_addr);
      } else {
        assert(0); /* gethostbyname() returned a bizarre addrtype */
      }

      return (0);
    }

    return (err == TRY_AGAIN) ? 1 : -1;
#endif
  }
}

/** Copy a box_addr_t from <b>src</b> to <b>dest</b>.
 */
void box_addr_copy(boxaddr *dest, const boxaddr *src)
{
  if (src == dest)
    return;

  assert(src);
  assert(dest);

  memcpy(dest, src, sizeof(boxaddr));
}

 /** Parse an address or address-port combination from <b>s</b>, resolve the
 * address as needed, and put the result in <b>addr_out</b> and (optionally)
 * <b>port_out</b>.  Return 0 on success, negative on failure. */
int box_addr_port_lookup(const char *s, boxaddr *addr_out, uint16_t *port_out)
{
  const char *port;
  boxaddr addr;
  uint16_t portval;
  char *tmp = NULL;

  assert(s);
  assert(addr_out);

  s = eat_whitespace(s);

  if (*s == '[') {
    port = strstr(s, "]");
    if (!port)
      goto err;
    tmp = box_strndup(s + 1, (size_t)(port - (s + 1)));
    port = port+1;
    if (*port == ':')
      port++;
    else
      port = NULL;
  } else {
    port = strchr(s, ':');
    if (port)
      tmp = box_strndup(s, (size_t)(port - s));
    else
      tmp = box_strdup(s);
    if (port)
      ++port;
  }

  if (box_getaddrinfo(tmp, AF_UNSPEC, &addr) != 0)
    goto err;

  FREE(tmp);

  if (port) {
    portval = (uint16_t) parse_long(port, 10, 1, 65535, NULL, NULL);
    if (!portval)
      goto err;
  } else {
    portval = 0;
  }

  if (port_out)
    *port_out = portval;

  box_addr_copy(addr_out, &addr);

  return (0);

 err:
  FREE(tmp);

  return (-1);
}


/** Set <b>dest</b> to equal the IPv6 address in the 16 bytes at
 * <b>ipv6_bytes</b>. */
STATIC void box_addr_from_ipv6_bytes(boxaddr *dest, const char *ipv6_bytes)
{
  assert(dest);
  assert(ipv6_bytes);

  memset(dest, 0, sizeof(boxaddr));
  dest->family = AF_INET6;
  memcpy(dest->addr.in6_addr.s6_addr, ipv6_bytes, 16);
}

/** Set <b>dest</b> equal to the IPv6 address in the in6_addr <b>in6</b>. */
STATIC void box_addr_from_in6(boxaddr *dest, const struct in6_addr *in6)
{
  box_addr_from_ipv6_bytes(dest, (const char*)in6->s6_addr);
}

/** Set <b>dest</b> to equal the IPv4 address in <b>v4addr</b> (given in
 * network order). */
STATIC void box_addr_from_ipv4n(boxaddr *dest, uint32_t v4addr)
{
  assert(dest);
  memset(dest, 0, sizeof(boxaddr));
  dest->family = AF_INET;
  dest->addr.in_addr.s_addr = v4addr;
}

socklen_t box_addr_to_sockaddr(const boxaddr *a, uint16_t port,
    struct sockaddr *sa_out, socklen_t len)
{
  struct sockaddr_in *sin;
  struct sockaddr_in6 *sin6;

  memset(sa_out, 0, len);

  sa_family_t family = box_addr_family(a);

  if (family == AF_INET) {
    if (len < (int)sizeof(struct sockaddr_in))
      return (0);

    sin = (struct sockaddr_in *)sa_out;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
    sin->sin_len = sizeof(struct sockaddr_in);
#endif
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    sin->sin_addr.s_addr = box_addr_to_ipv4n(a);

    return sizeof(struct sockaddr_in);
  } else if (family == AF_INET6) {
    if (len < (int)sizeof(struct sockaddr_in6))
      return (0);

    sin6 = (struct sockaddr_in6 *)sa_out;
#ifdef HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN
    sin6->sin6_len = sizeof(struct sockaddr_in6);
#endif
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port = htons(port);
    memcpy(&sin6->sin6_addr, box_addr_to_in6(a), sizeof(struct in6_addr));

    return sizeof(struct sockaddr_in6);
  } else {
    return (0);
  }
}

/** Return a string representing the address <b>addr</b>.  This string
 * is statically allocated, and must not be freed.  Each call to
 * <b>fmt_addr_impl</b> invalidates the last result of the function.
 * This function is not thread-safe.
 */
const char * fmt_addr(const boxaddr *addr)
{
  static char buf[BOX_ADDR_BUF_LEN];
  if (!addr)
    return ("<null>");
  if (box_addr_to_str(buf, addr, sizeof(buf)))
    return (buf);
  else
    return ("???");
}

/** Convert a boxaddr <b>addr</b> into a string, and store it in
 *  <b>dest</b> of size <b>len</b>.  Returns a pointer to dest on success,
 *  or NULL on failure.
 */
const char * box_addr_to_str(char *dest, const boxaddr *addr, socklen_t len)
{
  const char *ptr;
  assert(addr && dest);

  switch (box_addr_family(addr)) {
    case AF_INET:
      /* Shortest addr x.x.x.x + \0 */
      if (len < 8)
        return (NULL);
      ptr = inet_ntop(AF_INET, &addr->addr.in_addr, dest, len);
      break;
    case AF_INET6:
      /* Shortest addr [ :: ] + \0 */
      if (len < 3)
        return (NULL);

      ptr = inet_ntop(AF_INET6, &addr->addr.in6_addr, dest, len);

      break;
    case AF_UNIX:
      snprintf(dest, len, "AF_UNIX");
      ptr = dest;
      break;
    default:
      return (NULL);
  }

  return (ptr);
}
