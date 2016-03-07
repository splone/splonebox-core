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

#include <stdlib.h>
#include <uv.h>

#include "sb-common.h"
#include "rpc/sb-rpc.h"

static void write_cb(uv_write_t *req, int status);

outputstream *outputstream_new(uint32_t maxmem)
{
  outputstream *ws = MALLOC(outputstream);

  if (ws == NULL)
    return (NULL);

  ws->maxmem = maxmem;
  ws->stream = NULL;
  ws->curmem = 0;

  return (ws);
}


void outputstream_set(outputstream *ostream, uv_stream_t *stream)
{
  streamhandle_set_outputstream((uv_handle_t *)stream, ostream);
  ostream->stream = stream;
}


void outputstream_free(outputstream *ostream)
{
  FREE(ostream);
}


int outputstream_write(outputstream *ostream, char *buffer, size_t len)
{
  uv_buf_t buf;
  uv_write_t *req;
  struct write_request_data *data;

  if ((ostream->curmem + len) > ostream->maxmem)
    return (-1);

  buf.base = buffer;
  buf.len = len;

  data = MALLOC(struct write_request_data);

  if (data == NULL)
    return (-1);

  data->ostream = ostream;
  data->buffer = buffer;
  data->len = len;
  req = MALLOC(uv_write_t);

  if (req == NULL)
    return (-1);

  req->data = data;
  ostream->curmem += len;

  if (uv_write(req, ostream->stream, &buf, 1, write_cb) != 0)
    return (-1);

  return (0);
}


static void write_cb(uv_write_t *req, int status)
{
  struct write_request_data *data = req->data;

  if (status == -1) {
    LOG("error on write");
    return;
  }

  FREE(req);
  data->ostream->curmem -= data->len;
  FREE(data);
}
