/*  mtop
 *  Copyright (C) 2013  Toon Schoenmakers
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "debug.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>

#include <event2/event.h>

static const struct option options[] = {
  { "help",         no_argument,       0, 'h' },
  { "debug",        optional_argument, 0, 'D' },
  { 0, 0, 0, 0 }
};

struct event_base* event_base = NULL;

void onSignal(int signal) {
  event_base_free(event_base);
  exit(0);
};

int usage(char* program) {
  fprintf(stderr, "USAGE: %s [options]\n", program);
  fprintf(stderr, "-h, --help\tShow this help message.\n");
  fprintf(stderr, "-D, --debug\tIncrease the debug level\n");
  fprintf(stderr, "           \tYou can also directly set a certain debug level with -D5 where 5 is the debug level.\n");
  return 0;
};

int main(int argc, char** argv) {
  int arg, optindex;
  while ((arg = getopt_long(argc, argv, "hD::", options, &optindex)) != -1) {
    switch (arg) {
    case 'h':
      return usage(argv[0]);
    case 'D':
      if (optarg) {
        errno = 0;
        long tmp = strtol(optarg, NULL, 0);
        if (errno == 0 && tmp < 256 && tmp >= 0)
          debug = (unsigned char) tmp;
        else {
          fprintf(stderr, "%ld is an invalid debug level.\n", tmp);
          return 1;
        }
      } else
        debug++;
    }
  }
  event_base = event_base_new();
  signal(SIGTERM, onSignal);
  signal(SIGINT, onSignal);
  event_base_dispatch(event_base);
  return 0;
};