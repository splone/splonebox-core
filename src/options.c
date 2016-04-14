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
 *    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 */

#include <assert.h>
#include <unistd.h>
#include "sb-common.h"
#include "options.h"

/** Return the offset of <b>member</b> within the type <b>tp</b>, in bytes */
#if defined(__GNUC__) && __GNUC__ > 3
#define STRUCT_OFFSET(tp, member) __builtin_offsetof(tp, member)
#else
 #define STRUCT_OFFSET(tp, member)                                            \
   ((off_t) (((char*)&((tp*)0)->member)-(char*)0))
#endif

/** A list of abbreviations and aliases to map command-line options, obsolete
 * option names, or alternative option names, to their current values. */
static configabbrev option_abbrevs_[] = {
  { "Contact", "ContactInfo", 0, 0},
  { NULL, NULL, 0, 0},
};

/** An entry for config_vars: "The option <b>name</b> has type
 * CONFIG_TYPE_<b>conftype</b>, and corresponds to
 * options.<b>member</b>"
 */
#define VAR(name,conftype,member,initvalue)                                   \
  { name, CONFIG_TYPE_ ## conftype, STRUCT_OFFSET(options, member),           \
      initvalue }
/** As VAR, but the option name and member name are the same. */
#define V(member,conftype,initvalue)                                          \
  VAR(#member, conftype, member, initvalue)
/** An entry for config_vars: "The option <b>name</b> is obsolete." */
#define OBSOLETE(name) { name, CONFIG_TYPE_OBSOLETE, 0, NULL }
#define VPORT(member,conftype,initvalue)                                      \
  VAR(#member, conftype, member ## _lines, initvalue)

static configvar option_vars_[] = {
  V(ApiTransportListen,         STRING, NULL),
  V(ApiNamedPipeListen,         FILENAME, NULL),
  V(RedisDatabaseListen,        STRING, NULL),
  V(RedisDatabaseAuth,          STRING, NULL),
  V(ContactInfo,                STRING,   NULL),
  { NULL, CONFIG_TYPE_OBSOLETE, 0, NULL }
};

/** Magic value for options. */
#define OR_OPTIONS_MAGIC 9090909

/** Configuration format for options. */
static configformat options_format = {
  sizeof(options),
  OR_OPTIONS_MAGIC,
  STRUCT_OFFSET(options, magic_),
  option_abbrevs_,
  option_vars_,
  NULL
};

static options *global_options = NULL;

int options_init_from_boxrc(void)
{
   // load boxrc from disk
   char *cf=NULL;
   int retval = -1;

   cf = load_boxrc_from_disk();

   // options init from string
   retval = options_init_from_string(cf);

   FREE(cf);

   return retval;
}

options * options_get(void)
{
  assert(global_options);
  return global_options;
}

void options_free(options *options)
{
  confparse_free(&options_format, options);
  global_options = NULL;
}

/** Load the options from the configuration in <b>cf</b>, validate
 * them for consistency and take actions based on them.
 *
 * Return 0 if success, negative on error:
 *  * -1 for general errors.
 *  * -2 for failure to parse/validate,
 *  * -3 for transition not allowed
 *  * -4 for error while setting the new options
 */
STATIC setoptionerror options_init_from_string(const char *cf)
{
  options *newoptions;
  configline *cl;
  int retval;
  setoptionerror err = SETOPT_ERR_MISC;

  newoptions = CALLOC(1, options);
  newoptions->magic_ = OR_OPTIONS_MAGIC;
  options_init(newoptions);

  const char *body = cf;

  if (!body)
    goto err;
  /* get config lines, assign them */
  retval = confparse_get_lines(body, &cl, 1);

  if (retval < 0) {
    err = SETOPT_ERR_PARSE;
    goto err;
  }

  retval = confparse_assign(&options_format, newoptions, cl, 0, 0);
  confparse_free_lines(cl);

  if (retval < 0) {
    err = SETOPT_ERR_PARSE;
    goto err;
  }

  /* Validate newoptions */
  if (options_validate(newoptions) < 0) {
    err = SETOPT_ERR_PARSE; /*XXX make this a separate return value.*/
    goto err;
  }

  global_options = newoptions;

  return SETOPT_OK;

 err:
  confparse_free(&options_format, newoptions);
  LOG_WARNING("Failed to parse/validate config.");

  return err;
}

STATIC char * load_boxrc_from_disk(void)
{
  int fd;
  char *string = NULL;
  struct stat statbuf;

  fd = filesystem_open_read(".boxrc");

  if (fstat(fd, &statbuf) < 0) {
    return (NULL);
  }

  if ((uint64_t)(statbuf.st_size)+1 >= ((size_t)(SSIZE_MAX-16))) {
    close(fd);
    errno = EINVAL;
    return (NULL);
  }

  string = MALLOC_ARRAY((size_t)(statbuf.st_size+1), char);
  ssize_t r = filesystem_read_all(fd, string, (size_t)statbuf.st_size);

  if (r < 0) {
    int save_errno = errno;
    FREE(string);
    close(fd);
    errno = save_errno;
    return (NULL);
  }

  string[statbuf.st_size] = '\0';

  close(fd);

  return string;
}

STATIC int options_validate(options *options)
{
  if (options->ApiTransportListen && options->ApiNamedPipeListen) {
    LOG_WARNING("You cannot set both ApiTransportListen and ApiNamedPipeListen.\
        ");
    return (-1);
  }

  if (options->ApiTransportListen) {
    if (box_addr_port_lookup(options->ApiTransportListen,
        &options->ApiTransportListenAddr,
        &options->ApiTransportListenPort) < 0) {
      LOG_WARNING("ApiTransportListen failed to parse or resolve. Please fix.");
      return (-1);
    }

    options->apitype = SERVER_TYPE_TCP;
  }

  if (options->RedisDatabaseListen) {
    if (box_addr_port_lookup(options->RedisDatabaseListen,
        &options->RedisDatabaseListenAddr,
        &options->RedisDatabaseListenPort) < 0) {
      LOG_WARNING("RedisDatabaseListen failed to parse or resolve. Please fix. \
          ");
      return (-1);
    }
  }

  if (options->ApiNamedPipeListen) {
    options->apitype = SERVER_TYPE_PIPE;
  }

  if (options->ContactInfo) {
    // do we need additional checks here for this string
  }

  return (0);
}

/** Set <b>options</b> to hold reasonable defaults for most options.
 * Each option defaults to zero. */
STATIC void options_init(options *options)
{
  confparse_init(&options_format, options);
}
