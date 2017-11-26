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

#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <uv.h>

#include "sb-common.h"
#include "rpc/sb-rpc.h"
#include "rpc/connection/inputstream.h"

inputstream *inputstream_new(inputstream_cb cb, uint32_t buffer_size,
    void *data)
{
  inputstream *rs = malloc_or_die(sizeof(inputstream));

  rs->data = data;
  rs->size = 0;
  rs->cb = cb;
  rs->stream = NULL;
  rs->free_handle = false;

  /* initialize circular buffer */
  rs->circbuf_start = malloc_array_or_die(buffer_size, sizeof(unsigned char));
  rs->circbuf_read_pos = rs->circbuf_start;
  rs->circbuf_write_pos = rs->circbuf_start;
  rs->circbuf_end = rs->circbuf_start + buffer_size;

  return (rs);
}


void inputstream_set(inputstream *istream, uv_stream_t *stream)
{
  streamhandle_set_inputstream((uv_handle_t *)stream, istream);
  istream->stream = stream;
}


void inputstream_start(inputstream *istream)
{
  sbassert(istream);
  sbassert(istream->stream);

  uv_read_start(istream->stream, inputstream_alloc_cb, inputstream_read_cb);
}


void inputstream_stop(inputstream *istream)
{
  uv_read_stop(istream->stream);
}


void inputstream_free(inputstream *istream)
{
  sbassert(istream);
  sbassert(istream->stream);

  if (istream->free_handle)
    uv_close((uv_handle_t *)istream->stream, inputstream_close_cb);

  FREE(istream->circbuf_start);
  FREE(istream);
}


size_t inputstream_pending(inputstream *istream)
{
  return (istream->size);
}

unsigned char *inputstream_get_read(inputstream *istream, size_t *read_count)
{
  if (!inputstream_pending(istream)) {
    *read_count = 0;
    return NULL;
  }

  if (istream->circbuf_read_pos < istream->circbuf_write_pos)
    *read_count = (size_t) (istream->circbuf_write_pos - istream->circbuf_read_pos);
  else
    *read_count = (size_t) (istream->circbuf_end - istream->circbuf_read_pos);

  return istream->circbuf_read_pos;
}


size_t inputstream_read(inputstream *istream, unsigned char *buf, size_t count)
{
  size_t dst_size = count;
  size_t size = dst_size;
  size_t recent;
  size_t infinite = 1;
  size_t size_count;
  unsigned char *rptr;

  for (recent = 0; infinite; infinite = 0) {
    for (rptr = inputstream_get_read(istream, &recent); istream->size;
        rptr = inputstream_get_read(istream, &recent)) {
      size_count = MIN(dst_size, recent);

      memcpy(buf, rptr, size_count);

      size_t capacity = (size_t)(istream->circbuf_end - istream->circbuf_start);
      istream->circbuf_read_pos += size_count;

      if (istream->circbuf_read_pos >= istream->circbuf_end)
        istream->circbuf_read_pos -= capacity;

      bool full = (capacity == istream->size);
      istream->size -= size_count;

      if (full) {
        inputstream_start(istream);
      }

      if (!(dst_size -= size_count)) {
        return size;
      }

      buf += size_count;
    }
  }

  return size - dst_size;
}


STATIC void inputstream_close_cb(uv_handle_t *handle)
{
  FREE(handle->data);
  FREE(handle);
}


STATIC void inputstream_alloc_cb(uv_handle_t *handle,
    UNUSED(size_t suggested_size), uv_buf_t *buf)
{
  inputstream *istream = streamhandle_get_inputstream(handle);

  if (istream->size == (size_t)(istream->circbuf_end -
      istream->circbuf_start)) {
    buf->len = 0;
    return;
  }

  if (istream->circbuf_write_pos >= istream->circbuf_read_pos)
    buf->len = (size_t)(istream->circbuf_end - istream->circbuf_write_pos);
  else
    buf->len = (size_t)(istream->circbuf_read_pos - istream->circbuf_write_pos);

  buf->base = (char*) istream->circbuf_write_pos;
}


STATIC void inputstream_read_cb(uv_stream_t *stream, ssize_t nread,
    UNUSED(const uv_buf_t *buf))
{
  size_t read;
  inputstream *istream;

  istream = streamhandle_get_inputstream((uv_handle_t *)stream);

  /*
   * nread is > 0 if there is data available, 0 if libuv is done reading for
   * now, or < 0 on error.
   */
  if (nread <= 0) {
    if (nread != UV_ENOBUFS) {
      /* no buffer space available */
      uv_read_stop(stream);
      /* close connection */
      istream->cb(istream, istream->data, true);
    }
    return;
  }

  read = (size_t)nread;

  istream->circbuf_write_pos += read;

  if (istream->circbuf_write_pos >= istream->circbuf_end)
    istream->circbuf_write_pos -= (size_t)(istream->circbuf_end - istream->circbuf_start);

  istream->size += read;

  if (!((size_t)(istream->circbuf_end - istream->circbuf_start) - istream->size)) {
    inputstream_stop(istream);
  }

  istream->cb(istream, istream->data, false);
}
