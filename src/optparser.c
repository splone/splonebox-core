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

#include <getopt.h>

#include "sb-common.h"
#include "version.h"
#include "optparser.h"

STATIC void print_usage(string name)
{
  LOG("Usage: %s [OPTION]\n\n"
    "Mandatory arguments to long options are mandatory for short options too.\n"
    "  -h, --help print this message\n"
    "  -v         print verbose. Increase verbosity with multiple 'v'.\n"
    "  --version  print version\n", name.str);
}

STATIC void print_version(void)
{
  LOG("%s\n", VERSION);
}

void optparser(int argc, char **argv)
{
  verbose_level = VERBOSE_OFF;
  int option_index = 0;
  int c;

  string name = cstring_to_string(argv[0]);

  struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {0, 0, 0, 0}
  };

  while (1){
    c = getopt_long(argc, argv, "hvV", long_options, &option_index);

    if (c == -1)
      break;

    switch (c){

    case 'h':
      print_usage(name);
      exit(0);
    case 'V':
      print_version();
      exit(0);
    case 'v':
      verbose_level++;
      break;
    default:
      LOG_ERROR("Illegal option passed.\n");
      print_usage(name);
      abort();
    }
  }

}
