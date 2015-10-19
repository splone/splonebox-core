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
#include <msgpack/object.h>
#include <msgpack/pack.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "rpc/sb-rpc.h"
#include "sb-common.h"


/*
 * Message Format:
 *
 * - Request -
 *
 * msgpack - Array
 * ----------------------------------------------------
 * | 0 (type) | id (request_id) | method | arguments  |
 * ----------------------------------------------------
 *
 * - Response -
 *
 * msgpack - Array
 * --------------------------------------------------------------------
 * | 1 (type) | id (request_id) | Error Code or Null | result or Null |
 * --------------------------------------------------------------------
 */

/* Functions */

int message_unpack_apikey(msgpack_object *obj, struct message_request *req,
    struct api_error *api_error);
int message_unpack_type(msgpack_object *obj, struct message_request *req,
    struct api_error *api_error);
int message_unpack_validate_type(msgpack_object *ptr,
    struct api_error *api_error);
int message_unpack_validate_msgid(msgpack_object *ptr,
    struct api_error *api_error);
int message_unpack_validate_apikey(msgpack_object *obj,
    struct api_error *api_error);
int message_unpack_validate_status(msgpack_object *ptr,
    struct api_error *api_error);
int message_unpack_validate_result(msgpack_object *ptr,
    struct api_error *api_error);
int message_unpack_msgid(msgpack_object *obj, struct message_request *req,
    struct api_error *api_error);
int message_unpack_status(msgpack_object *obj, struct message_response *res,
    struct api_error *api_error);
int message_unpack_result(msgpack_object *obj, struct message_request *req,
    struct api_error *api_error);
int message_unpack_method(msgpack_object *obj, struct message_request *req,
    struct api_error *api_error);
int message_unpack_validate_method(msgpack_object *ptr,
    struct api_error *api_error);
int message_unpack_validate_args(msgpack_object *ptr,
    struct api_error *api_error);
int message_unpack_args(msgpack_object *obj, struct message_request *req,
    struct message_params_object *params, struct api_error *api_error);
int message_pack_apikey(msgpack_packer *pk, char *key);
int message_pack_type(msgpack_packer *pk, uint8_t type);
int message_pack_method(msgpack_packer *pk, char *method);
int message_pack_params(msgpack_packer *pk,
    struct message_params_object *params);
int message_pack_msgid(msgpack_packer *pk, uint32_t msgid);



string unpack_string(msgpack_object *obj);
char * unpack_bin(msgpack_object *obj);
int64_t unpack_int(msgpack_object *obj);
uint64_t unpack_uint(msgpack_object *obj);
bool unpack_boolean(msgpack_object *obj);
double unpack_float(msgpack_object *obj);
int unpack_params(msgpack_object *obj, struct message_params_object *params);



int pack_string(msgpack_packer *pk, string str);
int pack_uint8(msgpack_packer *pk, uint8_t uinteger);
int pack_uint16(msgpack_packer *pk, uint16_t uinteger);
int pack_uint32(msgpack_packer *pk, uint32_t uinteger);
int pack_uint64(msgpack_packer *pk, uint64_t uinteger);
int pack_int8(msgpack_packer *pk, int8_t integer);
int pack_int16(msgpack_packer *pk, int16_t integer);
int pack_int32(msgpack_packer *pk, int32_t integer);
int pack_int64(msgpack_packer *pk, int64_t integer);
int pack_nil(msgpack_packer *pk);
int pack_bool(msgpack_packer *pk, bool boolean);
int pack_float(msgpack_packer *pk, double floating);
int pack_params(msgpack_packer *pk, struct message_params_object params);
