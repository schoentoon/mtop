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

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct config* config = NULL;

int parse_config(char* config_file) {
  FILE* f = fopen(config_file, "r");
  if (!f) {
    fprintf(stderr, "Error '%s' while opening '%s'\n", strerror(errno), config_file);
    return 1;
  }
  char line_buffer[BUFSIZ];
  config = malloc(sizeof(struct config));
  memset(config, 0, sizeof(struct config));
  unsigned int line_count = 0;
  while (fgets(line_buffer, sizeof(line_buffer), f)) {
    line_count++;
    if (strlen(line_buffer) == 1 || line_buffer[0] == '#')
      continue;
    char key[BUFSIZ];
    char value[BUFSIZ];
    if (sscanf(line_buffer, "%[a-z_] = %[^\t\n]", key, value) == 2) {
      if (strcmp(key, "listen_port") == 0) {
        struct listener* l = new_listener(value);
        if (l) {
          if (!config->listeners)
            config->listeners = l;
          else {
            struct listener* ll = config->listeners;
            while (ll->next)
              ll = ll->next;
            ll->next = l;
          }
        }
      }
    } else {
      fprintf(stderr, "Error on line %zd\tCould not be parsed correctly.\n", line_count);
      return 1;
    }
  };
  return 0;
};

int dispatch_config(struct event_base* event_base) {
  if (config->listeners) {
    struct listener* l = config->listeners;
    while (l) {
      if (initListener(event_base, l))
        return 1;
      l = l->next;
    }
  }
  return 0;
};