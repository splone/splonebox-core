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

#pragma once

#define STRINGIFY(v) STR_VALUE(v)
#define STR_VALUE(arg) #arg

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_REVISION 7

#define VERSION STRINGIFY(VERSION_MAJOR) "." \
                STRINGIFY(VERSION_MINOR) "." \
                STRINGIFY(VERSION_REVISION)
