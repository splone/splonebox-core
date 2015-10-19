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

#include <stdlib.h>
#include <uv.h>

#include "sb-common.h"
#include "rpc/sb-rpc.h"

struct streamhandle {
  inputstream * istream;
  outputstream *ostream;
};

static struct streamhandle *init_streamhandle(uv_handle_t *handle);

void streamhandle_set_inputstream(uv_handle_t *handle, inputstream *istream)
{
  struct streamhandle *hd = init_streamhandle(handle);

  hd->istream = istream;
}


inputstream *streamhandle_get_inputstream(uv_handle_t *handle)
{
  struct streamhandle *hd = init_streamhandle(handle);
  inputstream *rs = hd->istream;

  return (rs);
}


void streamhandle_set_outputstream(uv_handle_t *handle,
    outputstream *ostream)
{
  struct streamhandle *hd = init_streamhandle(handle);

  hd->ostream = ostream;
}


static struct streamhandle *init_streamhandle(uv_handle_t *handle)
{
  struct streamhandle *hd;

  if (handle->data == NULL) {
    hd = MALLOC(struct streamhandle);

    if (hd == NULL)
      return (NULL);

    hd->istream = NULL;
    hd->ostream = NULL;
    handle->data = hd;
  } else
    hd = handle->data;

  return (hd);
}
