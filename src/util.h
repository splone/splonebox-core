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

STATIC int digit_to_num(char d);
STATIC int scan_unsigned(const char **bufp, unsigned long *out, int width,
    int base);
STATIC int scan_signed(const char **bufp, long *out, int width);
STATIC int scan_double(const char **bufp, double *out, int width);
STATIC int scan_string(const char **bufp, char *out, int width);
