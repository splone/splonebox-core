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

/* Typedefs */
typedef struct outputstream   outputstream;
typedef struct inputstream inputstream;
typedef void (*inputstream_cb)(inputstream *inputstream, void *data, bool eof);
typedef struct api_event api_event;
typedef struct equeue equeue;
typedef struct queue_entry queue_entry;
typedef struct message_object message_object;
typedef struct connection_request_event_info connection_request_event_info;


#define MESSAGE_REQUEST_ARRAY_SIZE 4
#define MESSAGE_RESPONSE_ARRAY_SIZE 4
#define MESSAGE_TYPE_REQUEST 0
#define MESSAGE_TYPE_RESPONSE 1
#define MESSAGE_RESPONSE_UNKNOWN UINT32_MAX
#define MESSAGE_APIKEY_LENGTH 64



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
  OBJECT_TYPE_NIL,
  OBJECT_TYPE_INT,
  OBJECT_TYPE_UINT,
  OBJECT_TYPE_BOOL,
  OBJECT_TYPE_FLOAT,
  OBJECT_TYPE_STR,
  OBJECT_TYPE_BIN,
  OBJECT_TYPE_ARRAY,
} message_object_type;

/* Structs */

struct message_params_object {
  message_object *obj;
  size_t size;
};

struct message_object {
  message_object_type type;
  union {
    int64_t integer;
    uint64_t uinteger;
    string string;
    char *bin;
    bool boolean;
    double floating;
    struct message_params_object params;
  } data;
};

struct message_request {
  uint8_t type;
  uint32_t msgid;
  string method;
  struct message_params_object params;
};

struct message_response {
  uint32_t msgid;
  struct message_params_object params;
};

struct connection {
  uint32_t msgid;
  msgpack_unpacker *mpac;
  msgpack_sbuffer *sbuf;
  bool closed;
  equeue *queue;
  struct {
    inputstream *read;
    outputstream *write;
    uv_stream_t *uv;
  } streams;
  struct {
    size_t n;
    size_t m;
    struct callinfo **a;
  } callvector;
};

struct callinfo {
  uint32_t msgid;
  bool hasresponse;
  bool errorresponse;
  struct message_response *response;
};

struct dispatch_info {
  int (*func)(
    connection_request_event_info *info
    );
  bool async;
  string name;
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
  uv_file fd;
  inputstream_cb cb;
  size_t buffer_size;
  size_t rpos;
  size_t cpos;
  bool reading;
};

/* this structure holds a request and all information to send a response */
struct connection_request_event_info {
  struct connection *con;
  struct message_request *request;
  struct dispatch_info *dispatcher;
  struct api_error api_error;
};

/* this structure holds the request event information and a callback to a
   handler function */
struct api_event {
  connection_request_event_info info;
  void (*handler)(connection_request_event_info *info);
};

TAILQ_HEAD(queue, queue_entry);

struct equeue {
  struct queue head;
  equeue *root;
};

struct queue_entry {
  union {
    equeue *queue;
    struct {
      queue_entry *root;
      api_event event;
    } entry;
  } data;
  TAILQ_ENTRY(queue_entry) node;
};

/* define global root event queue */
extern equeue *equeue_root;


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

struct callinfo * connection_send_request(string pluginlongtermpk, string method,
    struct message_params_object *params, struct api_error *api_error);
int connection_send_response(struct connection *con, uint32_t msgid,
    struct message_params_object *params, struct api_error *api_error);
int connection_hashmap_put(string pluginlongtermpk, struct connection *con);

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
 * Read data from the `inputstream` instance into a buffer
 *
 * @param inputstream The `inputstream` instance
 * @param buf The buffer which will receive the data
 * @param count The number of bytes to copy
 * @return The number of bytes copied
 */
size_t inputstream_read(inputstream *inputstream, char *buf, size_t count);

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

/**
 * Stop listening on the address specified by `endpoint`
 *
 * @param endpoint Address of the server (tcp address, unix socket or named
 *                 pipe)
 * @return 0 if successful, -1 otherwise
 */
int server_stop(string endpoint);
int server_close(void);

int event_initialize(void);

/**
 * create a new queue or child queue
 *
 * @params root if NULL a root queue is created, if not NULL a child queue for
 *              `root` is created
 * @returns queue instance
 */
equeue * equeue_new(equeue *root);

/**
 * get a queue element
 *
 * this function first checks if the passed queue is empty, if yes it return
 * an empty event object. if it's not empty the first queue entry will be
 * returned and removed from the queue.
 *
 * if the queue entry is a child, the first child queue entry is returned and
 * removed from the child queue. after that, the child queue entry associated
 * event is returned.
 *
 * if the queue entry is no child. the queue entry associated event is returned
 *
 * @param queue queue instance
 */
api_event equeue_get(equeue *queue);

/**
 * put an event in a queue
 *
 * @param queue queue instance
 * @param event event to enqueue
 */
int equeue_put(equeue *queue, api_event event);

/**
 * run all events of a queue
 *
 * @params queue queue instance
 */
void equeue_run_events(equeue *queue);

/**
 * free queue
 *
 * @params queue queue instance
 */
void equeue_free(equeue *queue);

bool equeue_empty(equeue *queue);




void streamhandle_set_inputstream(uv_handle_t *handle,
    inputstream *inputstream);
void streamhandle_set_outputstream(uv_handle_t *handle,
    outputstream *outputstream);
inputstream * streamhandle_get_inputstream(uv_handle_t *handle);


int dispatch_table_init(void);
int dispatch_table_free(void);
struct dispatch_info *dispatch_table_get(string method);
void dispatch_table_put(string method, struct dispatch_info *info);
int handle_run(connection_request_event_info *info);
int handle_register(connection_request_event_info *info);
int handle_error(connection_request_event_info *info);


/* Message Functions */
void free_params(struct message_params_object params);
bool message_is_request(msgpack_object *obj);
bool message_is_response(msgpack_object *obj);
int message_serialize_error_response(msgpack_packer *pk, struct api_error *err,
    uint32_t msgid);
int message_serialize_response(struct message_response *res,
    msgpack_packer *pk);
struct message_request *message_deserialize_request(msgpack_object *obj,
                                                    struct api_error *err);
struct message_response *message_deserialize_response(msgpack_object *obj,
    struct api_error *api_error);
struct message_response *message_deserialize_error_response(msgpack_object *obj,
    struct api_error *api_error);
int message_serialize_request(struct message_request *req, msgpack_packer *pk);
void message_dispatch(msgpack_object *req, msgpack_packer *res);
uint64_t message_get_id(msgpack_object *obj);
bool message_is_error_response(msgpack_object *obj);


/* DB functions */

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
extern int db_function_add(string apikey, struct message_params_object *func);

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
  struct message_params_object *args);

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
