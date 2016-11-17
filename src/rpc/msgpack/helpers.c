#include "rpc/sb-rpc.h"
#include "api/helpers.h"
#include "rpc/msgpack/helpers.h"
#include "sb-common.h"

STATIC bool msgpack_rpc_to_string(const msgpack_object *const obj,
    string *const arg);
STATIC bool msgpack_rpc_is_notification(msgpack_object *req);
STATIC msgpack_object *msgpack_rpc_msg_id(msgpack_object *req);

typedef struct {
  const msgpack_object *mobj;
  object *aobj;
  bool container;
  size_t idx;
} msgpack_to_api_object_stack_item;

bool msgpack_rpc_to_object(const msgpack_object *const obj, object *const arg)
{
  bool ret = true;
  kvec_t(msgpack_to_api_object_stack_item) stack = KV_INITIAL_VALUE;
  kv_push(stack, ((msgpack_to_api_object_stack_item) { obj, arg, false, 0 }));

  while (ret && kv_size(stack)) {
    msgpack_to_api_object_stack_item cur = kv_last(stack);

    if (!cur.container) {
      *cur.aobj = NIL;
    }

    switch (cur.mobj->type) {
      case MSGPACK_OBJECT_NIL: {
        break;
      }
      case MSGPACK_OBJECT_BOOLEAN: {
        *cur.aobj = BOOLEAN_OBJ(cur.mobj->via.boolean);
        break;
      }
      case MSGPACK_OBJECT_NEGATIVE_INTEGER: {
        sbassert(sizeof(int64_t) == sizeof(cur.mobj->via.i64));
        *cur.aobj = INTEGER_OBJ(cur.mobj->via.i64);
        break;
      }
      case MSGPACK_OBJECT_POSITIVE_INTEGER: {
        sbassert(sizeof(uint64_t) == sizeof(cur.mobj->via.u64));
        *cur.aobj = UINTEGER_OBJ(cur.mobj->via.u64);
        break;
      }
      case MSGPACK_OBJECT_FLOAT: {
        sbassert(sizeof(double) == sizeof(cur.mobj->via.f64));
        *cur.aobj = FLOATING_OBJ(cur.mobj->via.f64);
        break;
      }
      case MSGPACK_OBJECT_STR: {
        *cur.aobj = STRING_OBJ(((string) {
          .length = cur.mobj->via.str.size,
          .str = (cur.mobj->via.str.ptr == NULL || cur.mobj->via.str.size == 0
              ? NULL : box_strndup(cur.mobj->via.str.ptr,
              cur.mobj->via.str.size)),
        }));
        break;
      }
      case MSGPACK_OBJECT_BIN: {
        *cur.aobj = STRING_OBJ(((string) {
          .length = cur.mobj->via.bin.size,
          .str = (cur.mobj->via.bin.ptr == NULL || cur.mobj->via.bin.size == 0
              ? NULL : box_strndup(cur.mobj->via.bin.ptr, cur.mobj->via.bin.size)),
        }));
        break;
      }

      case MSGPACK_OBJECT_ARRAY: {
        const size_t size = cur.mobj->via.array.size;

        if (cur.container) {
          if (cur.idx >= size) {
            (void)kv_pop(stack);
          } else {
            const size_t idx = cur.idx;
            cur.idx++;
            kv_last(stack) = cur;
            kv_push(stack, ((msgpack_to_api_object_stack_item) {
              .mobj = &cur.mobj->via.array.ptr[idx],
              .aobj = &cur.aobj->data.array.items[idx],
              .container = false,
            }));
          }
        } else {
          *cur.aobj = ARRAY_OBJ(((array) {
            .size = size,
            .capacity = size,
            .items = (size > 0
                ? calloc(size, sizeof(*cur.aobj->data.array.items)) : NULL),
          }));
          cur.container = true;
          kv_last(stack) = cur;
        }
        break;
      }
      case MSGPACK_OBJECT_MAP: {
        const size_t size = cur.mobj->via.map.size;

        if (cur.container) {
          if (cur.idx >= size) {
            (void)kv_pop(stack);
          } else {
            const size_t idx = cur.idx;
            cur.idx++;
            kv_last(stack) = cur;
            const msgpack_object *const key = &cur.mobj->via.map.ptr[idx].key;
            switch (key->type) {
              case MSGPACK_OBJECT_STR: {
                cur.aobj->data.dictionary.items[idx].key = ((string) {
                  .length = key->via.str.size,
                  .str = (key->via.str.ptr == NULL || key->via.str.size == 0
                      ? NULL : box_strndup(key->via.str.ptr, key->via.str.size)),
                });
                break;
              }
              case MSGPACK_OBJECT_BIN: {
                cur.aobj->data.dictionary.items[idx].key = ((string) {
                  .length = key->via.bin.size,
                  .str = (key->via.bin.ptr == NULL || key->via.bin.size == 0
                      ? NULL : box_strndup(key->via.bin.ptr, key->via.bin.size)),
                });
                break;
              }
              case MSGPACK_OBJECT_NIL:
              case MSGPACK_OBJECT_BOOLEAN:
              case MSGPACK_OBJECT_POSITIVE_INTEGER:
              case MSGPACK_OBJECT_NEGATIVE_INTEGER:
              case MSGPACK_OBJECT_FLOAT:
              case MSGPACK_OBJECT_EXT:
              case MSGPACK_OBJECT_MAP:
              case MSGPACK_OBJECT_ARRAY: {
                ret = false;
                break;
              }
            }
            if (ret) {
              kv_push(stack, ((msgpack_to_api_object_stack_item) {
                .mobj = &cur.mobj->via.map.ptr[idx].val,
                .aobj = &cur.aobj->data.dictionary.items[idx].value,
                .container = false,
              }));
            }
          }
        } else {
          *cur.aobj = DICTIONARY_OBJ(((dictionary) {
            .size = size,
            .capacity = size,
            .items = (size > 0
                ? calloc(size, sizeof(*cur.aobj->data.dictionary.items))
                : NULL),
          }));
          cur.container = true;
          kv_last(stack) = cur;
        }
        break;
      }
      case MSGPACK_OBJECT_EXT: {
        break;
      }
    }
    if (!cur.container) {
      (void)kv_pop(stack);
    }
  }

  kv_destroy(stack);

  return ret;
}

STATIC bool msgpack_rpc_to_string(const msgpack_object *const obj,
                                  string *const arg)
{
  if (obj->type == MSGPACK_OBJECT_BIN || obj->type == MSGPACK_OBJECT_STR) {
    arg->str = obj->via.bin.ptr != NULL
                    ? box_strndup(obj->via.bin.ptr, obj->via.bin.size)
                    : NULL;
    arg->length = obj->via.bin.size;
    return true;
  }
  return false;
}

bool msgpack_rpc_to_array(const msgpack_object *const obj, array *const arg)
{
  if (obj->type != MSGPACK_OBJECT_ARRAY) {
    return false;
  }

  arg->size = obj->via.array.size;
  arg->items = CALLOC(obj->via.array.size, object);

  for (uint32_t i = 0; i < obj->via.array.size; i++) {
    if (!msgpack_rpc_to_object(obj->via.array.ptr + i, &arg->items[i])) {
      return false;
    }
  }

  return true;
}

bool msgpack_rpc_to_dictionary(const msgpack_object *const obj,
                               dictionary *const arg)
{
  if (obj->type != MSGPACK_OBJECT_MAP) {
    return false;
  }

  arg->size = obj->via.array.size;
  arg->items = CALLOC(obj->via.map.size, key_value_pair);


  for (uint32_t i = 0; i < obj->via.map.size; i++) {
    if (!msgpack_rpc_to_string(&obj->via.map.ptr[i].key,
          &arg->items[i].key)) {
      return false;
    }

    if (!msgpack_rpc_to_object(&obj->via.map.ptr[i].val,
          &arg->items[i].value)) {
      return false;
    }
  }

  return true;
}

void msgpack_rpc_from_boolean(bool result, msgpack_packer *res)
{
  if (result) {
    msgpack_pack_true(res);
  } else {
    msgpack_pack_false(res);
  }
}

void msgpack_rpc_from_integer(int64_t result, msgpack_packer *res)
{
  msgpack_pack_int64(res, result);
}

void msgpack_rpc_from_uinteger(uint64_t result, msgpack_packer *res)
{
  msgpack_pack_uint64(res, result);
}

void msgpack_rpc_from_float(double result, msgpack_packer *res)
{
  msgpack_pack_double(res, result);
}

void msgpack_rpc_from_string(string result, msgpack_packer *res)
{
  msgpack_pack_str(res, result.length);
  msgpack_pack_str_body(res, result.str, result.length);
}

typedef struct {
  const object *obj;
  bool container;
  size_t idx;
} api_to_msgpack_object_stack_item;

void msgpack_rpc_from_object(const object result, msgpack_packer *const res)
{
  kvec_t(api_to_msgpack_object_stack_item) stack = KV_INITIAL_VALUE;
  kv_push(stack, ((api_to_msgpack_object_stack_item) {&result, false, 0}));

  while (kv_size(stack)) {
    api_to_msgpack_object_stack_item cur = kv_last(stack);

    switch (cur.obj->type) {
      case OBJECT_TYPE_NIL: {
        msgpack_pack_nil(res);
        break;
      }
      case OBJECT_TYPE_BOOL: {
        msgpack_rpc_from_boolean(cur.obj->data.boolean, res);
        break;
      }
      case OBJECT_TYPE_INT: {
        msgpack_rpc_from_integer(cur.obj->data.integer, res);
        break;
      }
      case OBJECT_TYPE_UINT: {
        msgpack_rpc_from_uinteger(cur.obj->data.uinteger, res);
        break;
      }
      case OBJECT_TYPE_FLOAT: {
        msgpack_rpc_from_float(cur.obj->data.floating, res);
        break;
      }
      case OBJECT_TYPE_STR: {
        msgpack_rpc_from_string(cur.obj->data.string, res);
        break;
      }
      case OBJECT_TYPE_ARRAY: {
        const size_t size = cur.obj->data.array.size;

        if (cur.container) {
          if (cur.idx >= size) {
            (void)kv_pop(stack);
          } else {
            const size_t idx = cur.idx;
            cur.idx++;
            kv_last(stack) = cur;
            kv_push(stack, ((api_to_msgpack_object_stack_item) {
              .obj = &cur.obj->data.array.items[idx],
              .container = false,
            }));
          }
        } else {
          msgpack_pack_array(res, size);
          cur.container = true;
          kv_last(stack) = cur;
        }
        break;
      }
      case OBJECT_TYPE_DICTIONARY: {
        const size_t size = cur.obj->data.dictionary.size;
        if (cur.container) {
          if (cur.idx >= size) {
            (void)kv_pop(stack);
          } else {
            const size_t idx = cur.idx;
            cur.idx++;
            kv_last(stack) = cur;
            msgpack_rpc_from_string(cur.obj->data.dictionary.items[idx].key,
                res);
            kv_push(stack, ((api_to_msgpack_object_stack_item) {
              .obj = &cur.obj->data.dictionary.items[idx].value,
              .container = false,
            }));
          }
        } else {
          msgpack_pack_map(res, size);
          cur.container = true;
          kv_last(stack) = cur;
        }
        break;
      }
    }
    if (!cur.container) {
      (void)kv_pop(stack);
    }
  }
  kv_destroy(stack);
}

void msgpack_rpc_from_array(array result, msgpack_packer *res)
{
  msgpack_pack_array(res, result.size);

  for (size_t i = 0; i < result.size; i++) {
    msgpack_rpc_from_object(result.items[i], res);
  }
}

void msgpack_rpc_from_dictionary(dictionary result, msgpack_packer *res)
{
  msgpack_pack_map(res, result.size);

  for (size_t i = 0; i < result.size; i++) {
    msgpack_rpc_from_string(result.items[i].key, res);
    msgpack_rpc_from_object(result.items[i].value, res);
  }
}

void msgpack_rpc_serialize_request(uint64_t request_id,
                                   string method,
                                   array args,
                                   msgpack_packer *pac)
{
  msgpack_pack_array(pac, request_id ? 4 : 3);
  msgpack_pack_int(pac, request_id ? 0 : 2);

  if (request_id) {
    msgpack_pack_uint64(pac, request_id);
  }

  msgpack_rpc_from_string(method, pac);
  msgpack_rpc_from_array(args, pac);
}

void msgpack_rpc_serialize_response(uint64_t response_id,
                                    struct api_error *err,
                                    object arg,
                                    msgpack_packer *pac)
{
  msgpack_pack_array(pac, 4);
  msgpack_pack_int(pac, 1);
  msgpack_pack_uint64(pac, response_id);

  if (err->isset) {
    // error represented by a [type, message] array
    msgpack_pack_array(pac, 2);
    msgpack_rpc_from_integer(err->type, pac);
    msgpack_rpc_from_string(cstring_to_string(err->msg), pac);
    // Nil result
    msgpack_pack_nil(pac);
  } else {
    // Nil error
    msgpack_pack_nil(pac);
    // Return value
    msgpack_rpc_from_object(arg, pac);
  }
}

STATIC bool msgpack_rpc_is_notification(msgpack_object *req)
{
  return req->via.array.ptr[0].via.u64 == 2;
}

msgpack_object *msgpack_rpc_method(msgpack_object *req)
{
  msgpack_object *obj = req->via.array.ptr
    + (msgpack_rpc_is_notification(req) ? 1 : 2);
  return obj->type == MSGPACK_OBJECT_STR || obj->type == MSGPACK_OBJECT_BIN ?
    obj : NULL;
}

msgpack_object *msgpack_rpc_args(msgpack_object *req)
{
  msgpack_object *obj = req->via.array.ptr
    + (msgpack_rpc_is_notification(req) ? 2 : 3);
  return obj->type == MSGPACK_OBJECT_ARRAY ? obj : NULL;
}

STATIC msgpack_object *msgpack_rpc_msg_id(msgpack_object *req)
{
  if (msgpack_rpc_is_notification(req)) {
    return NULL;
  }
  msgpack_object *obj = &req->via.array.ptr[1];
  return obj->type == MSGPACK_OBJECT_POSITIVE_INTEGER ? obj : NULL;
}

void msgpack_rpc_validate(uint64_t *response_id,
                          msgpack_object *req,
                          struct api_error *err)
{
  // response id not known yet

  *response_id = UINT64_MAX;
  // Validate the basic structure of the msgpack-rpc payload
  if (req->type != MSGPACK_OBJECT_ARRAY) {
    error_set(err, API_ERROR_TYPE_VALIDATION, "Message is not an array");
    return;
  }

  if (req->via.array.size == 0) {
    error_set(err, API_ERROR_TYPE_VALIDATION, "Message is empty");
    return;
  }

  if (req->via.array.ptr[0].type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
    error_set(err, API_ERROR_TYPE_VALIDATION, "Message type must be an integer");
    return;
  }

  uint64_t type = req->via.array.ptr[0].via.u64;
  if (type != MESSAGE_TYPE_REQUEST && type != MESSAGE_TYPE_NOTIFICATION) {
    error_set(err, API_ERROR_TYPE_VALIDATION, "Unknown message type");
    return;
  }

  if ((type == MESSAGE_TYPE_REQUEST && req->via.array.size != 4)
      || (type == MESSAGE_TYPE_NOTIFICATION && req->via.array.size != 3)) {
    error_set(err, API_ERROR_TYPE_VALIDATION,
        "Request array size should be 4 (request) or 3 (notification)");
    return;
  }

  if (type == MESSAGE_TYPE_REQUEST) {
    msgpack_object *id_obj = msgpack_rpc_msg_id(req);
    if (!id_obj) {
      error_set(err, API_ERROR_TYPE_VALIDATION, "ID must be a positive integer");
      return;
    }
    *response_id = id_obj->via.u64;
  }

  if (!msgpack_rpc_method(req)) {
    error_set(err, API_ERROR_TYPE_VALIDATION, "Method must be a string");
    return;
  }

  if (!msgpack_rpc_args(req)) {
    error_set(err, API_ERROR_TYPE_VALIDATION, "Parameters must be an array");
    return;
  }
}
