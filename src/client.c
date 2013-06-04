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

#include "client.h"

#include "debug.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <event2/buffer.h>

struct client* new_client() {
  struct client* client = malloc(sizeof(struct client));
  memset(client, 0, sizeof(struct client));
  return client;
};

void client_readcb(struct bufferevent* bev, void* context) {
  struct client* client = context;
  struct evbuffer* input = bufferevent_get_input(bev);
  struct evbuffer* output = bufferevent_get_output(bev);
  size_t len;
  char* line = evbuffer_readln(input, &len, EVBUFFER_EOL_ANY);
  while (line) {
    DEBUG(255, "Raw line: %s", line);
    char buf[65];
    if (strcmp(line, "PROTOCOLS") == 0)
      send_loaded_modules_info(bev);
    else if (sscanf(line, "ENABLE %64s", buf) == 1) {
      struct module* module = get_module(buf);
      if (module) {
        struct enabled_mod* em = malloc(sizeof(struct enabled_mod));
        em->module = module;
        em->id = 0;
        em->next = NULL;
        if (!client->mods) {
          client->mods = em;
          evbuffer_add_printf(output, "LOADED %s WITH ID %d\n", buf, em->id);
        } else {
          em->id++;
          struct enabled_mod* lm = client->mods;
          do {
            if (lm->module == em->module) {
              evbuffer_add_printf(output, "ALREADY LOADED %s WITH ID %d\n", em->module->name, em->id);
              free(em);
              lm = NULL;
              break;
            }
            em->id = lm->id + 1;
            lm = lm->next;
          } while (lm);
          if (lm) {
            lm->next = em;
            evbuffer_add_printf(output, "LOADED %s WITH ID %d\n", buf, em->id);
          }
        }
      } else
        evbuffer_add_printf(output, "UNABLE TO LOAD %s\n", buf);
    } else if (sscanf(line, "DISABLE %64s", buf) == 1) {
      struct module* module = get_module(buf);
      if (module) {
        struct enabled_mod* lm = client->mods;
        if (lm->module == module) {
          client->mods = lm->next;
          evbuffer_add_printf(output, "DISABLED %s WITH ID %d\n", buf, lm->id);
          free(lm);
        } else {
          while (lm->next) {
            if (lm->next->module == module) {
              struct enabled_mod* to_free = lm->next;
              lm->next = lm->next->next;
              evbuffer_add_printf(output, "DISABLED %s WITH ID %d\n", buf, to_free->id);
              free(to_free);
              break;
            }
            lm = lm->next;
          };
        }
      } else
        evbuffer_add_printf(output, "MODULE %s DOESN'T EXIST\n", buf);
    }
    free(line);
    line = evbuffer_readln(input, &len, EVBUFFER_EOL_ANY);
  };
};

void client_eventcb(struct bufferevent* bev, short events, void* context) {
  if (!(events & BEV_EVENT_CONNECTED)) {
    struct client* client = context;
    if (client->mods) {
      struct enabled_mod* node = client->mods;
      while (node) {
        struct enabled_mod* next = node->next;
        free(node);
        node = next;
      };
    }
    free(client);
    bufferevent_free(bev);
  }
};