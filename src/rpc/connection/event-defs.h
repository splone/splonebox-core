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

#include <stdarg.h>
#include "sb-common.h"

#define EVENT_HANDLER_MAX_ARGC 6

typedef void (*argv_callback)(void **argv);
typedef struct message {
  int priority;
  argv_callback handler;
  void *argv[EVENT_HANDLER_MAX_ARGC];
} event;

typedef void(*event_scheduler)(event e, void *data);

#define VA_EVENT_INIT(event, p, h, a) \
  do { \
    sbassert(a <= EVENT_HANDLER_MAX_ARGC); \
    (event)->priority = p; \
    (event)->handler = h; \
    if (a) { \
      va_list args; \
      va_start(args, a); \
      for (int i = 0; i < a; i++) { \
        (event)->argv[i] = va_arg(args, void *); \
      } \
      va_end(args); \
    } \
  } while (0)

static inline event event_create(int priority, argv_callback cb, int argc, ...)
{
  sbassert(argc <= EVENT_HANDLER_MAX_ARGC);
  event e;
  VA_EVENT_INIT(&e, priority, cb, argc);
  return e;
}
