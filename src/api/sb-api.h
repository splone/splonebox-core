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

#pragma once

#include <stdbool.h>
#include <msgpack.h>

#include "sb-common.h"
#include "rpc/sb-rpc.h"

/* Functions */

/**
 * Registers a plugin e.g. by storing relevant information in database.
 * @param[in] name    name of the plugin
 * @param[in] desc    description of the plugin
 * @param[in] author  author of the plugin
 * @param[in] license the license of the plugin
 * @param[in] funcs   the actual functions to register
 * @return 0 in case of success otherwise 1
 */
int api_register(string api_key, string name, string desc, string author,
    string license, struct message_params_object functions,
    struct api_error *api_error);

/**
 * Run a plugin function
 * @param[in] apikey    apikey of the plugin
 * @param[in] function_name    function of the plugin
 * @param[in] args  function arguments of the plugin
 * @param[in] pk   msgpack packer instance
 * @param[in] api_error   api_error instance
 * @return 0 in case of success otherwise -1
 */
int api_run(string api_key, string function_name, uint64_t callid,
    struct message_params_object args, msgpack_packer *pk,
    struct api_error *api_error);

/**
 * Generates an API key using /dev/urandom. The length of the key
 * depends on the length of the string.
 * @param[in] key string to store the API key.
 * @return 0 in case of success otherwise -1
 */
int api_get_key(string key);

int api_register_response(uint32_t msgid, msgpack_packer *pk);
