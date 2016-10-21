#pragma once

#include "rpc/sb-rpc.h"

bool msgpack_rpc_to_object(const msgpack_object *const obj, object *const arg);

//STATIC bool msgpack_rpc_to_string(const msgpack_object *const obj,
//    string *const arg);

bool msgpack_rpc_to_array(const msgpack_object *const obj, array *const arg);

bool msgpack_rpc_to_dictionary(const msgpack_object *const obj,
    dictionary *const arg);

void msgpack_rpc_from_boolean(bool result, msgpack_packer *res);

void msgpack_rpc_from_integer(int64_t result, msgpack_packer *res);
void msgpack_rpc_from_uinteger(uint64_t result, msgpack_packer *res);

void msgpack_rpc_from_float(double result, msgpack_packer *res);

void msgpack_rpc_from_string(string result, msgpack_packer *res);

void msgpack_rpc_from_object(const object result, msgpack_packer *const res);

void msgpack_rpc_from_array(array result, msgpack_packer *res);

void msgpack_rpc_from_dictionary(dictionary result, msgpack_packer *res);

void msgpack_rpc_serialize_request(uint64_t request_id, string method,
    array args, msgpack_packer *pac);

void msgpack_rpc_serialize_response(uint64_t response_id, struct api_error *err,
    object arg, msgpack_packer *pac);

//STATIC bool msgpack_rpc_is_notification(msgpack_object *req);

msgpack_object *msgpack_rpc_method(msgpack_object *req);

msgpack_object *msgpack_rpc_args(msgpack_object *req);

//STATIC msgpack_object *msgpack_rpc_msg_id(msgpack_object *req);

void msgpack_rpc_validate(uint64_t *response_id, msgpack_object *req,
    struct api_error *err);
