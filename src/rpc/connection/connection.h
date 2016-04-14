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

#pragma once

#include "rpc/sb-rpc.h"

STATIC void close_cb(uv_handle_t *handle);
STATIC int connection_handle_request(struct connection *con,
    msgpack_object *obj);
STATIC int connection_handle_response(struct connection *con,
    msgpack_object *obj);
STATIC void connection_request_event(connection_request_event_info *info);
STATIC void connection_close(struct connection *con);
STATIC int parse_cb(inputstream *istream, void *data, bool eof);
