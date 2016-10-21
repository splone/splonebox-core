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

#include "sb-common.h"
#include "rpc/db/sb-db.h"

struct function {
  string name;
  string description;
  string args[2];
};

struct plugin {
  string key;
  string name;
  string description;
  string author;
  string license;
  struct function *function;
  uint64_t callid;
};

void connect_to_db(void);
void connect_and_create(char *apikey);
int validate_run_request(const unsigned long data1, const unsigned long data2);
int validate_crypto_cookie_packet(unsigned char *buffer, uint64_t length);
int validate_crypto_write(unsigned char *buffer, uint64_t length);
void register_test_function(void);
struct plugin *helper_get_example_plugin(void);
void helper_free_plugin(struct plugin *p);
void helper_register_plugin(struct plugin *p);
