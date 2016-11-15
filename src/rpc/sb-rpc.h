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
 *
 *    This file incorporates code covered by the following terms:
 *
 *    Copyright Neovim contributors. All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <msgpack.h>
#include <msgpack/pack.h>
#include <stdbool.h>
#include <hiredis/hiredis.h>

#include "sb-common.h"
#include "tweetnacl.h"

/* Typedefs */
typedef struct outputstream   outputstream;
typedef struct inputstream inputstream;
typedef void (*inputstream_cb)(inputstream *inputstream, void *data, bool eof);
typedef struct api_event api_event;
typedef struct message_object message_object;
typedef struct connection_request_event_info connection_request_event_info;


#define MESSAGE_REQUEST_ARRAY_SIZE 4
#define MESSAGE_RESPONSE_ARRAY_SIZE 4
#define MESSAGE_RESPONSE_UNKNOWN UINT32_MAX

#define METHOD_MAXLEN 512

#define STREAM_BUFFER_SIZE 0xffff

#define CALLINFO_INIT (struct callinfo) {0, false, false, NIL}


/*
 * Structure Information:
 *
 * { (message_object)
 *   type: Array
 *   data: [obj] (size is count of objects)
 * }
 */

/* Enums */

typedef enum {
  MESSAGE_TYPE_REQUEST,
  MESSAGE_TYPE_RESPONSE,
  MESSAGE_TYPE_NOTIFICATION
} message_type;

typedef struct object object;

typedef enum {
  OBJECT_TYPE_NIL,
  OBJECT_TYPE_INT,
  OBJECT_TYPE_UINT,
  OBJECT_TYPE_BOOL,
  OBJECT_TYPE_FLOAT,
  OBJECT_TYPE_STR,
  OBJECT_TYPE_ARRAY,
  OBJECT_TYPE_DICTIONARY
} object_type;

typedef enum {
  TUNNEL_INITIAL,
  TUNNEL_COOKIE_SENT,
  TUNNEL_ESTABLISHED
} crypto_state;

/* Structs */

typedef struct key_value_pair key_value_pair;

typedef struct {
  object *items;
  size_t size;
  size_t capacity;
} array;

typedef struct {
  key_value_pair *items;
  size_t size;
  size_t capacity;
} dictionary;

struct object {
  object_type type;
  union {
    int64_t integer;
    uint64_t uinteger;
    string string;
    char *bin;
    bool boolean;
    double floating;
    array array;
    dictionary dictionary;
  } data;
};

struct message_request {
  uint32_t msgid;
  string method;
  array params;
};

struct message_response {
  uint32_t msgid;
  array params;
};

struct key_value_pair {
  string key;
  object value;
};


/*****************************************************************************
 * The following crypto_context structure should be considered PRIVATE to    *
 * the rpc connection layer. No non-rpc connection layer code should be      *
 * using this structure in any way.                                          *
 *****************************************************************************/
#define PLUGINKEY_SIZE 8
#define PLUGINKEY_STRING_SIZE ((PLUGINKEY_SIZE * 2) + 1)
#define CLIENTLONGTERMPK_ARRAY_SIZE 32

typedef object (*apidispatchwrapper)(uint64_t con_id, uint64_t msgid,
    char *pluginkey, array args, struct api_error *error);

typedef struct {
  apidispatchwrapper func;
  bool async;
  string name;
} dispatch_info;

/* hashmap declarations */
MAP_DECLS(uint64_t, string)
MAP_DECLS(string, ptr_t)
MAP_DECLS(uint64_t, ptr_t) /* maps callid <> pluginkey */
MAP_DECLS(cstr_t, ptr_t) /* maps pluginkey <> connection */
MAP_DECLS(string, dispatch_info)
MAP_DECLS(cstr_t, uint64_t)

struct crypto_context {
  crypto_state state;
  uint64_t nonce;
  uint64_t receivednonce;
  unsigned char clientshortservershort[32];
  unsigned char clientshorttermpk[32];
  unsigned char servershorttermpk[32];
  unsigned char servershorttermsk[32];
  unsigned char minutekey[32];
  unsigned char lastminutekey[32];
  char pluginkeystring[PLUGINKEY_STRING_SIZE];
};

typedef struct wbuffer wbuffer;

struct wbuffer {
  size_t size;
  size_t refcount;
  char *data;
};

struct callinfo {
  uint64_t msgid;
  bool returned;
  bool errored;
  object result;
};

struct outputstream {
  uv_stream_t *stream;
  size_t curmem;
  uint32_t maxmem;
};

struct write_request_data {
  outputstream *ostream;
  char *buffer;
  size_t len;
};

struct inputstream {
  void * data;
  char * buffer;
  uv_stream_t * stream;
  inputstream_cb cb;
  unsigned char *circbuf_start;
  unsigned char *circbuf_end;
  unsigned char *circbuf_read_pos;
  unsigned char *circbuf_write_pos;
  size_t size;
  bool free_handle;
};

/* this structure holds a request and all information to send a response */
struct connection_request_event_info {
  struct connection *con;
  dispatch_info dispatcher;
  array args;
  uint64_t msgid;
};

/* Functions */

/**
 * Initialize a `struct connection` instance
 *
 * @return 0 on success, -1 otherwise
 */
int connection_init(void);

/**
 * Create a API connection from a libuv stream (tcp or pipe/socket client
 * connection)
 *
 * @param stream The `uv_stream_t` instance
 * @return 0 on success, -1 otherwise
 */
int connection_create(uv_stream_t *stream);

object connection_send_request(char *pluginkey, string method,
    array args, struct api_error *err);
int connection_send_response(uint64_t con_id, uint32_t msgid,
    object arg, struct api_error *api_error);
void connection_hashmap_put(uint64_t id, struct connection *con);
void pluginkeys_hashmap_put(char *pluginkey, uint64_t id);
void loop_wait_for_response(struct connection *con,
    struct callinfo *cinfo);
int connection_teardown(void);
void connection_subscribe(uint64_t id, char *event);
void connection_unsubscribe(uint64_t id, char *event);
bool connection_send_event(uint64_t id, char *name, array args);

/**
 * Create a new `outputstream` instance. A `outputstream` instance contains the
 * logic to write to a libuv stream
 *
 * @param maxmem Maximum amount of memory used by the new `outputstream` instance
 * @return The created `outputstream` instance
 */
outputstream *outputstream_new(uint32_t maxmem);

/**
 * Associate a `uv_stream_t` instance
 *
 * @param outputstream The `outputstream` instance
 * @param stream The `uv_stream_t` instance
 */
void outputstream_set(outputstream *outputstream, uv_stream_t *stream);

/**
 * Free the memory of the `outputstream` instance
 *
 * @param outputstream The `outputstream` instance
 */
void outputstream_free(outputstream *outputstream);

int outputstream_write(outputstream *outputstream, char *buffer, size_t len);

/**
 * Create a new inputstream instance. A inputstream contains the logic to read
 * from a libuv stream.
 *
 * @param cb Callback function that will be called when data is available
 * @param buffer_size Size of the internal Buffer
 * @param data An object or state to associate with the inputstream instance
 * @return The created `inputstream` instance
 */
inputstream *inputstream_new(inputstream_cb cb, uint32_t buffer_size,
    void *data);

/**
 * Associate a `uv_stream_t` instance
 *
 * @param inputstream The `inputstream` instance
 * @param stream The `uv_stream_t` instance
 */
void inputstream_set(inputstream *inputstream, uv_stream_t *stream);

/**
 * Begin watching for events from a `inputstream` instance
 *
 * @param inputstream The `inputstream` instance
 */
void inputstream_start(inputstream *inputstream);

/**
 * Stop watching fro events from a `inputstream` instance
 *
 * @param inputstream The `inputstream` instance
 */
void inputstream_stop(inputstream *inputstream);

/**
 * Free the memory of the `inputstream` instance
 *
 * @param inputstream The `inputstream` instance
 */
void inputstream_free(inputstream *inputstream);

/**
 * Return the number of bytes pending for reading for the `inputstream` instance
 *
 * @param inputstream The `inputstream` instance
 */
size_t inputstream_pending(inputstream *inputstream);

/**
 * Get the number of bytes that can be read from the `inputstream` instance
 * and the start pointer to the data that can be read
 *
 * @param inputstream The `inputstream` instance
 * @param read_count A pointer to an the read count integer variable

 * @return The data pointer
 */
unsigned char *inputstream_get_read(inputstream *istream, size_t *read_count);


/**
 * Read data from the `inputstream` instance into a buffer
 *
 * @param inputstream The `inputstream` instance
 * @param buf The buffer which will receive the data
 * @param count The number of bytes to copy
 * @return The number of bytes copied
 */
size_t inputstream_read(inputstream *inputstream, unsigned char *buf,
    size_t count);

/**
 * Initialize a Server Instance
 *
 * @return 0 on success, -1 otherwise
 */
int server_init(void);

/**
 * Listen on specified tcp/unix addresses for API calls. Addresses are
 * specified by `endpoint`. `endpoint` can be either a valid tcp address in the
 * ´ip:port´ format, a unix socket or a named pipe.
 *
 * @param endpoint Address of the server (tcp address, unix socket or named
 *                 pipe)
 * @returns 0 if successful, -1 otherwise.
 */
int server_start(string endpoint);
int server_start_tcp(boxaddr *addr, uint16_t port);
int server_start_pipe(char *name);

/**
 * Stop listening on the address specified by `endpoint`
 *
 * @param endpoint Address of the server (tcp address, unix socket or named
 *                 pipe)
 * @return 0 if successful, -1 otherwise
 */
int server_stop(char * endpoint);
int server_close(void);

void streamhandle_set_inputstream(uv_handle_t *handle,
    inputstream *inputstream);
void streamhandle_set_outputstream(uv_handle_t *handle,
    outputstream *outputstream);
inputstream * streamhandle_get_inputstream(uv_handle_t *handle);


int dispatch_table_init(void);
int dispatch_teardown(void);
dispatch_info dispatch_table_get(string method);
dispatch_info msgpack_rpc_get_handler_for(const char *name, size_t name_len);
void dispatch_table_put(string method, dispatch_info info);
object handle_run(uint64_t con_id, uint64_t msgid, char *pluginkey,
    array args, struct api_error *error);
object handle_result(uint64_t con_id, uint64_t msgid, char *pluginkey,
    array args, struct api_error *error);
object handle_register(uint64_t con_id, uint64_t msgid, char *pluginkey,
    array args, struct api_error *error);
object handle_subscribe(uint64_t con_id, uint64_t msgid, char *pluginkey,
    array args, struct api_error *error);
object handle_unsubscribe(uint64_t con_id, uint64_t msgid, char *pluginkey,
    array args, struct api_error *error);
object handle_broadcast(uint64_t con_id, uint64_t msgid, char *pluginkey,
    array args, struct api_error *error);
object msgpack_rpc_handle_invalid_arguments(uint64_t channel_id,
    uint64_t msgid, char *pluginkey, array args, struct api_error *error);
object msgpack_rpc_handle_missing_method(uint64_t channel_id,
    uint64_t msgid, char *pluginkey, array args, struct api_error *error);

/* Message Functions */
void free_params(array params);
bool message_is_request(msgpack_object *obj);
bool message_is_response(msgpack_object *obj);
int message_serialize_error_response(msgpack_packer *pk, struct api_error *err,
    uint32_t msgid);
int message_serialize_response(struct message_response *res,
    msgpack_packer *pk);
int message_deserialize_request(struct message_request *req,
    msgpack_object *obj, struct api_error *api_error);
int message_deserialize_response(struct message_response *res,
    msgpack_object *obj, struct api_error *api_error);
int message_deserialize_error_response(struct message_response *res,
    msgpack_object *obj, struct api_error *api_error);
int message_serialize_request(struct message_request *req, msgpack_packer *pk);
void message_dispatch(msgpack_object *req, msgpack_packer *res);
uint64_t message_get_id(msgpack_object *obj);
bool message_is_error_response(msgpack_object *obj);
struct message_object message_object_copy(struct message_object obj);
object copy_object(object obj);



/**
 * Currently only loads the server private key.
 *
 * @return 0 on success otherwise -1
 */
int crypto_init(void);

/**
 * Verfify if data has a message packet identifier and get the packet length
 *
 * @param cc The crypto_context connection crypto information (nonce etc.)
 * @param data Buffer containing a packet
 * @param[out] length The packet length
 * returns -1 in case of error otherwise 0
 */
 int crypto_verify_header(struct crypto_context *cc, unsigned char *data,
     uint64_t *length);

/**
 * Handle a client initiate packet
 *
 * @param cc The crypto_context connection crypto information (nonce etc.)
 * @param data Buffer containing a client initiate packet
 * returns -1 in case of error otherwise 0
 */
int crypto_recv_initiate(struct crypto_context *cc, unsigned char *data);

/**
 * Handle a client hello packet and send a server cookie packet as response
 *
 * @param cc The crypto_context connection crypto information (nonce etc.)
 * @param data Buffer containing a client hello packet
 * @param out The outputstream ready to write data
 * returns -1 in case of error otherwise 0
 */
int crypto_recv_hello_send_cookie(struct crypto_context *cc,
    unsigned char *data, outputstream *out);

/**
 * Handle a client message packet and unbox it's data
 *
 * @param cc The crypto_context connection crypto information (nonce etc.)
 * @param in Buffer containing a client message packet
 * @param[out] out Buffer for unboxed data
 * @param length The 'in' buffer length
 * @param[out] plaintextlen The packet length
 * returns -1 in case of error otherwise 0
 */
int crypto_read(struct crypto_context *cc, unsigned char *in, char *out,
    uint64_t length, uint64_t *plaintextlen);

/**
 * Box data into a server message packet send it
 *
 * @param cc The crypto_context connection crypto information (nonce etc.)
 * @param data Buffer containing data
 * @param length The 'data' buffer length
 * @param out The outputstream ready to write data
 * returns -1 in case of error otherwise 0
 */
int crypto_write(struct crypto_context *cc, char *data,
    size_t length, outputstream *out);

void crypto_update_minutekey(struct crypto_context *cc);

/**
 * Pack uint64_t into 8 byte
 *
 * @param y Buffer position
 * @param x The number
 */
void uint64_pack(unsigned char *y, uint64_t x);

/**
 * Unpack uint64_t from 8 byte
 *
 * @param x Buffer position
 * returns The unpacked number
 */
uint64_t uint64_unpack(const unsigned char *x);

/**
 * Check if bytes are equal (a == b)
 *
 * @param yv Buffer position a
 * @param ylen The length of a
 * @param xv Buffer position b
 * returns -1 in case of error otherwise 0
 */
int byte_isequal(const void *yv, long long ylen, const void *xv);

void msgpack_rpc_serialize_response(uint64_t response_id, struct api_error *err,
    object arg, msgpack_packer *pac);

void msgpack_rpc_serialize_request(uint64_t request_id, string method,
    array args, msgpack_packer *pac);
