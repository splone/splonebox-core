/**
 *    Copyright (C) 2016 splone UG
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

#pragma once

#include "sb-common.h"

/** A random-access array of one-bit-wide elements. */
typedef unsigned int bitarray_t;

STATIC void reset_line(const configformat *fmt, void *options,
    const char *key, int use_defaults);
STATIC void mark_lists_fragile(const configformat *fmt, void *options);
STATIC void reset(const configformat *fmt, void *options, const configvar *var,
    int use_defaults);
STATIC void clear(const configformat *fmt, void *options, const configvar *var);
STATIC void line_append(configline **lst, const char *key, const char *val);
STATIC int assign_line(const configformat *fmt, void *options, configline *c,
    int use_defaults, int clear_first, bitarray_t *options_seen);
STATIC const char * unescape_string(const char *s, char **result,
    size_t *size_out);
STATIC int assign_value(const configformat *fmt, void *options, configline *c);
STATIC configvar * find_option_mutable(const configformat *fmt,
    const char *key);
