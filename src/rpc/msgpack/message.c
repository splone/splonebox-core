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

#include <stdbool.h>
#include <stdlib.h>

#include "rpc/msgpack/sb-msgpack-rpc.h"
#include "sb-common.h"

bool message_is_request(msgpack_object *obj)
{
  if (!obj)
    return (false);

  /* check if type is mspack_object_params */
  if (obj->type != MSGPACK_OBJECT_ARRAY)
    return (false);

  msgpack_object_array *params = &obj->via.array;

  return (params->size == MESSAGE_REQUEST_ARRAY_SIZE &&
         params->ptr[0].type == MSGPACK_OBJECT_POSITIVE_INTEGER &&
         params->ptr[0].via.u64 == MESSAGE_TYPE_REQUEST);
}


bool message_is_response(msgpack_object *obj)
{
  if (!obj)
    return (false);

  /* check if type is mspack_object_array */
  if (obj->type != MSGPACK_OBJECT_ARRAY)
    return (false);

  msgpack_object_array *array = &obj->via.array;

  /* check if message is a response */
  return (array->size == MESSAGE_RESPONSE_ARRAY_SIZE &&
         array->ptr[0].type == MSGPACK_OBJECT_POSITIVE_INTEGER &&
         array->ptr[0].via.u64 == MESSAGE_TYPE_RESPONSE);
}


int message_serialize_error_response(msgpack_packer *pk,
    struct api_error *api_error, uint32_t msgid)
{
  struct message_object *err_data;
  array err_array;

  msgpack_pack_array(pk, 4);

  if (pack_uint8(pk, MESSAGE_TYPE_RESPONSE) == -1)
    return (-1);

  if (pack_uint32(pk, msgid) == -1)
    return (-1);

  err_array.size = 2;
  err_array.obj = CALLOC(2, struct message_object);

  if (!err_array.obj) {
    LOG_WARNING("Couldn't allocate memory for err_array object");
    return (-1);
  }

  err_data = &err_array.obj[0];
  err_data->type = OBJECT_TYPE_INT;
  err_data->data.integer = api_error->type;

  err_data = &err_array.obj[1];
  err_data->type = OBJECT_TYPE_STR;
  err_data->data.string = cstring_to_string(api_error->msg);

  if (!err_data->data.string.str) {
    LOG_WARNING("Couldn't allocate memory for string in err_array object");
    return (-1);
  }

  if (pack_params(pk, err_array) == -1)
    return (-1);

  if (pack_nil(pk) == -1)
    return (-1);

  return (true);
}


int message_serialize_response(struct message_response *res,
    msgpack_packer *pk)
{
  msgpack_pack_array(pk, 4);

  if (pack_uint8(pk, MESSAGE_TYPE_RESPONSE) == -1)
    return (-1);

  if (pack_uint32(pk, res->msgid) == -1)
    return (-1);

  if (pack_nil(pk) == -1)
    return (-1);

  if (pack_params(pk, res->params) == -1)
    return (-1);

  return (0);
}


int message_serialize_request(struct message_request *req,
    msgpack_packer *pk)
{
  msgpack_pack_array(pk, 4);

  if (pack_uint8(pk, req->type) == -1)
    return (-1);

  if (pack_uint32(pk, req->msgid) == -1)
    return (-1);

  if (pack_string(pk, req->method) == -1)
    return (-1);

  if (pack_params(pk, req->params) == -1)
    return (-1);

  return (0);
}


struct message_request *message_deserialize_request(msgpack_object *obj,
    struct api_error *api_error)
{
  msgpack_object *type, *msgid, *method, *params;
  struct message_request *req;
  uint64_t tmp_type;
  uint64_t tmp_msgid;

  req = MALLOC(struct message_request);

  if (!obj || !req || !api_error) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "Error");
    return (NULL);
  }

  /* type */
  if (obj->via.array.ptr[0].type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
    type = &obj->via.array.ptr[0];

    if (type) {
      tmp_type = unpack_uint(type);

      if ((tmp_type == MESSAGE_TYPE_REQUEST) ||
          (tmp_type == MESSAGE_TYPE_RESPONSE))
        req->type = (uint8_t)tmp_type;
      else {
        error_set(api_error, API_ERROR_TYPE_VALIDATION, "type must be 0 or 1");
        return (NULL);
      }
    } else {
      error_set(api_error, API_ERROR_TYPE_VALIDATION, "unpack type failed");
      return (NULL);
    }
  } else {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "type field has wrong type");
    return (NULL);
  }

  /* message id */
  msgid = &obj->via.array.ptr[1];

  if (!msgid || msgid->type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "illegal msgid");
    return (NULL);
  }

  tmp_msgid = unpack_uint(msgid);

  if (tmp_msgid >= UINT32_MAX) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "invalid msgid");
    return (NULL);
  }

  req->msgid = (uint32_t)tmp_msgid;

  /* method */
  if (obj->via.array.ptr[2].type == MSGPACK_OBJECT_STR) {
    method = &obj->via.array.ptr[2];

    if (method) {
      req->method = unpack_string(method);

      if (!req->method.str) {
        error_set(api_error, API_ERROR_TYPE_VALIDATION, "Error unpacking method");
        return (NULL);
      }
    } else {
      error_set(api_error, API_ERROR_TYPE_VALIDATION, "unpack method failed");
      return (NULL);
    }
  } else {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "method field has wrong type");
    return (NULL);
  }

  /* params */
  if (obj->via.array.ptr[3].type == MSGPACK_OBJECT_ARRAY) {
    params = &obj->via.array.ptr[3];

    if (params) {
      if (unpack_params(params, &req->params) == -1) {
        error_set(api_error, API_ERROR_TYPE_VALIDATION, "Error unpacking params");
        return (NULL);
      }
    } else {
      error_set(api_error, API_ERROR_TYPE_VALIDATION, "unpack params failed");
      return (NULL);
    }
  } else {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "params field has wrong type");
    return (NULL);
  }

  return (req);
}

bool message_is_error_response(msgpack_object *obj)
{
  return (obj->via.array.ptr[2].type != MSGPACK_OBJECT_NIL);
}

uint64_t message_get_id(msgpack_object *obj)
{
  return (obj->via.array.ptr[1].via.u64);
}

struct message_response *message_deserialize_response(msgpack_object *obj,
    struct api_error *api_error)
{
  msgpack_object *type, *msgid, *params;
  struct message_response *res;
  uint64_t tmp_msgid;

  res = MALLOC(struct message_response);

  if (!obj || !res || !api_error) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "Error");
    return (NULL);
  }

  /* type */
  if (obj->via.array.ptr[0].type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
    type = &obj->via.array.ptr[0];

    if (type) {
      if (unpack_uint(type) != MESSAGE_TYPE_RESPONSE) {
        error_set(api_error, API_ERROR_TYPE_VALIDATION, "type must be 1");
        return (NULL);
      }
    } else {
      error_set(api_error, API_ERROR_TYPE_VALIDATION, "unpack type failed");
      return (NULL);
    }
  } else {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "type field has wrong type");
    return (NULL);
  }

  /* message id */
  msgid = &obj->via.array.ptr[1];

  if (!msgid || msgid->type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "illegal msgid");
    return (NULL);
  }

  tmp_msgid = unpack_uint(msgid);

  if (tmp_msgid >= UINT32_MAX) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "invalid msgid");
    return (NULL);
  }

  res->msgid = (uint32_t)tmp_msgid;

  /* nil */
  if (obj->via.array.ptr[2].type != MSGPACK_OBJECT_NIL) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "nil field has wrong type");
    return (NULL);
  }

  /* params */
  if (obj->via.array.ptr[3].type == MSGPACK_OBJECT_ARRAY) {
    params = &obj->via.array.ptr[3];

    if (params) {
      if (unpack_params(params, &res->params) == -1) {
        error_set(api_error, API_ERROR_TYPE_VALIDATION, "Error unpacking params");
        return (NULL);
      }
    } else {
      error_set(api_error, API_ERROR_TYPE_VALIDATION, "unpack params failed");
      return (NULL);
    }
  } else {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "params field has wrong type");
    return (NULL);
  }

  return (res);
}

struct message_response *message_deserialize_error_response(msgpack_object *obj,
    struct api_error *api_error)
{
  msgpack_object *type, *msgid, *params;
  struct message_response *res;
  uint64_t tmp_msgid;

  res = MALLOC(struct message_response);

  if (!obj || !res || !api_error) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "Error");
    return (NULL);
  }

  /* type */
  if (obj->via.array.ptr[0].type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
    type = &obj->via.array.ptr[0];

    if (type) {
      if (unpack_uint(type) != MESSAGE_TYPE_RESPONSE) {
        error_set(api_error, API_ERROR_TYPE_VALIDATION, "type must be 1");
        return (NULL);
      }
    } else {
      error_set(api_error, API_ERROR_TYPE_VALIDATION, "unpack type failed");
      return (NULL);
    }
  } else {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "type field has wrong type");
    return (NULL);
  }

  /* message id */
  msgid = &obj->via.array.ptr[1];

  if (!msgid || msgid->type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "illegal msgid");
    return (NULL);
  }

  tmp_msgid = unpack_uint(msgid);

  if (tmp_msgid >= UINT32_MAX) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "invalid msgid");
    return (NULL);
  }

  res->msgid = (uint32_t)tmp_msgid;

  /* params */
  if (obj->via.array.ptr[2].type == MSGPACK_OBJECT_ARRAY) {
    params = &obj->via.array.ptr[2];

    if (params) {
      if (unpack_params(params, &res->params) == -1) {
        error_set(api_error, API_ERROR_TYPE_VALIDATION, "Error unpacking params");
        return (NULL);
      }
    } else {
      error_set(api_error, API_ERROR_TYPE_VALIDATION, "unpack params failed");
      return (NULL);
    }
  } else {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "params field has wrong type");
    return (NULL);
  }

  /* nil */
  if (obj->via.array.ptr[3].type != MSGPACK_OBJECT_NIL) {
    error_set(api_error, API_ERROR_TYPE_VALIDATION, "nil field has wrong type");
    return (NULL);
  }

  return (res);
}


static void free_message_object(message_object obj)
{
  switch (obj.type) {
  case OBJECT_TYPE_NIL:
    break;
  case OBJECT_TYPE_INT:
    break;
  case OBJECT_TYPE_UINT:
    break;
  case OBJECT_TYPE_BIN:
    /* FALLTHROUGH */
  case OBJECT_TYPE_STR:
    free_string(obj.data.string);
    break;
  case OBJECT_TYPE_BOOL:
    break;
  case OBJECT_TYPE_FLOAT:
    break;
  case OBJECT_TYPE_ARRAY:
    free_params(obj.data.params);
    break;
  default:
    return;
  }
}

struct message_object message_object_copy(struct message_object obj)
{
  switch (obj.type) {
  case OBJECT_TYPE_NIL:
    /* FALLTHROUGH */
  case OBJECT_TYPE_BOOL:
    /* FALLTHROUGH */
  case OBJECT_TYPE_INT:
    /* FALLTHROUGH */
  case OBJECT_TYPE_UINT:
    /* FALLTHROUGH */
  case OBJECT_TYPE_FLOAT:
    return obj;
  case OBJECT_TYPE_BIN:
    /* FALLTHROUGH */
  case OBJECT_TYPE_STR:
    return (struct message_object) {.type = OBJECT_TYPE_STR, .data.string =
        cstring_copy_string(obj.data.string.str) };
  case OBJECT_TYPE_ARRAY: {
    array array = ARRAY_INIT;

    for (size_t i = 0; i < obj.data.params.size; i++) {
      kv_push(struct message_object, array,
          message_object_copy(obj.data.params.obj[i]));
    }

    return (struct message_object) {.type = OBJECT_TYPE_ARRAY,
        .data.params = array};
  }
  default:
    abort();
  }
}


void free_params(array params)
{
  for (size_t i = 0; i < params.size; i++)
    free_message_object(params.obj[i]);

  if (params.obj)
    FREE(params.obj);
}
