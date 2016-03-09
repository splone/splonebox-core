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

#include "rpc/sb-rpc.h"


/* Extern */

extern redisContext *rc;

/* Functions */

/**
 * Connects to Redis database.
 * @param[in]   ip    ip address the redis server listens to
 * @param[in]   port  port the redis server listens to
 * @param[in]   tv    timeout for redis connection
 * @param[in]   password  password to authenticate against redis db
 * @return    0 on success otherwise -1
 */
extern int db_connect(string ip, int port, const struct timeval tv, string password);

/**
 * Disconnects from Redis db and frees Context object.
 */
extern void db_close(void);

/**
 * Stores a function in database associated with the corresponding module.
 * @param[in] apikey  key of the module that provides the corresponding
 *                    function
 * @param[in] func    array of functions to actually store
 * @return 0 on success otherwise -1
 */
extern int db_function_add(string apikey, array *func);

/**
 * Verifies whether the corresponding function is called correctly. To
 * do so, it verifies the name of the function and the arguments' type.
 * @param[in] apikey  key of the module that provides the corresponding
 *                    function
 * @param[in] name    name of the function to call
 * @param[in] args    function arguments
 * @return 0 if call is valid, otherwise -1
 */
extern int db_function_verify(string apikey, string name,
  array *args);

/**
 * Creates a plugin entry in the database and uses the API key as key.
 * @param[in] apikey buffer to contain the API key
 * @param[in] name    name of plugin
 * @param[in] desc    description of the plugin
 * @param[in] author  author of the plugin
 * @param[in] license the plugin's license text
 * returns -1 in case of error otherwise 0
 */
extern int db_plugin_add(string apikey, string name, string desc, string author,
    string license);

/**
 * Checks whether the passed API key is assigned to a plugin.
 * @param[in] apikey  key to check
 * returns 0 if key is valid otherwise -1
 */
extern int db_apikey_verify(string apikey);

/**
 * Stores an API key in the database.
 * @param[in] key to store
 * returns 0 on success otherwise -1
 */
extern int db_apikey_add(string apikey);
