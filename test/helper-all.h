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

void connect_to_db(void);
void connect_and_create(string apikey);
int validate_register_response(const unsigned long data1, const unsigned long data2);
int validate_run_response(const unsigned long data1, const unsigned long data2);
int validate_crypto_tunnel(char *buffer, uint64_t length);
int validate_crypto_write(char *buffer, uint64_t length);
