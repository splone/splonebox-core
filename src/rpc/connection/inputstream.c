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

#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <uv.h>

#include "sb-common.h"
#include "rpc/sb-rpc.h"

static void alloc_cb(uv_handle_t *, size_t, uv_buf_t *);
static void read_cb(uv_stream_t *, ssize_t, const uv_buf_t *);
static void close_cb(uv_handle_t *handle);

inputstream *inputstream_new(inputstream_cb cb, uint32_t buffer_size,
    void *data)
{
  inputstream *rs = MALLOC(inputstream);

  if (rs == NULL)
    return (NULL);

  rs->buffer = MALLOC_ARRAY(buffer_size, char);

  if (rs->buffer == NULL)
    return (NULL);

  rs->buffer_size = buffer_size;
  rs->data = data;
  rs->cb = cb;
  rs->stream = NULL;
  rs->rpos = 0;
  rs->cpos = 0;

  return (rs);
}


void inputstream_set(inputstream *istream, uv_stream_t *stream)
{
  streamhandle_set_inputstream((uv_handle_t *)stream, istream);
  istream->stream = stream;
}


void inputstream_start(inputstream *istream)
{
  istream->reading = false;
  uv_read_start(istream->stream, alloc_cb, read_cb);
}


void inputstream_stop(inputstream *istream)
{
  uv_read_stop(istream->stream);
}


void inputstream_free(inputstream *istream)
{
  uv_close((uv_handle_t *)istream->stream, close_cb);
}


size_t inputstream_pending(inputstream *istream)
{
  return (istream->cpos - istream->rpos);
}


size_t inputstream_read(inputstream *istream, char *buf, size_t size)
{
  if (size > 0) {
    memcpy(buf, istream->buffer + istream->rpos, size);
    istream->rpos += size;
  }

  if (istream->cpos == istream->buffer_size) {
    memmove(istream->buffer, istream->buffer + istream->rpos,
        istream->cpos - istream->rpos);

    istream->cpos -= istream->rpos;
    istream->rpos = 0;

    if (istream->cpos < istream->buffer_size)
      inputstream_start(istream);
  }

  return (size);
}


static void close_cb(uv_handle_t *handle)
{
  FREE(handle->data);
  FREE(handle);
}


static void alloc_cb(uv_handle_t *handle, UNUSED(size_t suggested_size),
    uv_buf_t *buf)
{
  inputstream *istream = streamhandle_get_inputstream(handle);

  if (istream->reading) {
    buf->len = 0;
    return;
  }

  /* available space */
  buf->len = istream->buffer_size - istream->cpos;
  /* first byte for write */
  buf->base = istream->buffer + istream->cpos;

  istream->reading = true;
}


static void read_cb(uv_stream_t *stream, ssize_t nread,
    UNUSED(const uv_buf_t *buf))
{
  size_t read;
  inputstream *istream =
      streamhandle_get_inputstream((uv_handle_t *)stream);

  /*
   * nread is > 0 if there is data available, 0 if libuv is done reading for
   * now, or < 0 on error.
   */
  if (nread <= 0) {
    if (nread != UV_ENOBUFS) {
      /* no buffer space available */
      uv_read_stop(stream);
      istream->cb(istream, istream->data, true);
    }
    return;
  }

  read = (size_t)nread;
  istream->cpos += read;

  if (istream->cpos == istream->buffer_size)
    inputstream_stop(istream);

  istream->reading = false;
  istream->cb(istream, istream->data, false);
}
