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
#include "websocket.h"

#include <stdio.h>
#include <stdlib.h>

#include <event2/buffer.h>
#include <ctype.h>

struct client* new_client() {
  struct client* client = malloc(sizeof(struct client));
  bzero(client, sizeof(struct client));
  return client;
};

static void client_timer(evutil_socket_t fd, short event, void* arg) {
  struct client* client = arg;
  struct enabled_mod* lm = client->mods;
  while (lm) {
    char databuf[BUFSIZ];
    if (update_value(lm->module, databuf, sizeof(databuf)))
      client_send_data(client, "%s: %s", lm->module->name, databuf);
    lm = lm->next;
  };
};

int process_line(struct client* client, char* line, size_t len) {
  DEBUG(255, "Raw line: %s", line);
  char buf[65];
  struct timeval tv = { 0, 0 };
  if (strcmp(line, "MODULES") == 0)
    send_loaded_modules_info(client);
  else if (sscanf(line, "ENABLE %64s", buf) == 1) {
    struct module* module = get_module(buf);
    if (module) {
      struct enabled_mod* em = malloc(sizeof(struct enabled_mod));
      em->module = module;
      em->id = 0;
      em->next = NULL;
      if (!client->mods) {
        client->mods = em;
        client_send_data(client, "LOADED %s WITH ID %d", buf, em->id);
      } else {
        em->id++;
        struct enabled_mod* lm = client->mods;
        if (lm->module == em->module) {
          client_send_data(client, "ALREADY LOADED %s WITH ID %d", em->module->name, em->id);
          free(em);
        } else {
          while (lm) {
            if (lm->module == em->module) {
              client_send_data(client, "ALREADY LOADED %s WITH ID %d", em->module->name, em->id);
              free(em);
              return 0;
            }
            em->id = lm->id + 1;
            if (!lm->next)
              break;
            lm = lm->next;
          };
          lm->next = em;
          client_send_data(client, "LOADED %s WITH ID %d", buf, em->id);
        }
      }
    } else
      client_send_data(client, "UNABLE TO LOAD %s", buf);
  } else if (sscanf(line, "DISABLE %64s", buf) == 1) {
    struct module* module = get_module(buf);
    if (module) {
      struct enabled_mod* lm = client->mods;
      if (lm->module == module) {
        client->mods = lm->next;
        client_send_data(client, "DISABLED %s WITH ID %d", buf, lm->id);
        free(lm);
      } else {
        while (lm->next) {
          if (lm->next->module == module) {
            struct enabled_mod* to_free = lm->next;
            lm->next = lm->next->next;
            client_send_data(client, "DISABLED %s WITH ID %d", buf, to_free->id);
            free(to_free);
            break;
          }
          lm = lm->next;
        };
      }
    } else
      client_send_data(client, "MODULE %s DOESN'T EXIST", buf);
  } else if (sscanf(line, "PULL %64s", buf) == 1) {
    struct module* module = get_module(buf);
    if (module) {
      char databuf[BUFSIZ];
      bzero(databuf, sizeof(databuf));
      if (update_value(module, databuf, sizeof(databuf)))
        client_send_data(client, "%s: %s", module->name, databuf);
    };
  } else if (sscanf(line, "INTERVAL %ld.%ld", &tv.tv_sec, &tv.tv_usec) == 2 || sscanf(line, "INTERVAL %ld", &tv.tv_sec) == 1) {
    if (client->timer)
      event_free(client->timer);
    client->timer = event_new(bufferevent_get_base(client->bev), -1, EV_PERSIST, client_timer, client);
    event_add(client->timer, &tv);
  } else if ((!client->websocket || !client->websocket->connected) && handle_handshake(client, line, len))
    return 0;
  else {
    switch (++client->unknown_command) {
    case 1:
      client_send_data(client, "This is not a valid command...");
      return 0;
    case 2:
      client_send_data(client, "I'm warning you, stop that.");
      return 0;
    case 3:
      client_eventcb(client->bev, BEV_ERROR, client);
      return 1;
    }
  }
  client->unknown_command = 0;
  return 0;
};

void client_send_data(struct client* client, char* line, ...) {
  va_list arg;
  va_start(arg, line);
  if (client->websocket && client->websocket->connected) {
    char data[1024*1024];
    size_t length = vsnprintf(data, sizeof(data), line, arg);
    encode_and_send_websocket(client, data, length);
  } else {
    struct evbuffer* output = bufferevent_get_output(client->bev);
    evbuffer_add_vprintf(output, line, arg);
    evbuffer_add(output, "\n", 2);
  }
  va_end(arg);
};

void client_readcb(struct bufferevent* bev, void* context) {
  struct client* client = context;
  struct evbuffer* input = bufferevent_get_input(bev);
  if (client->websocket && client->websocket->connected) {
    int res = 0;
    while (res == 0 && evbuffer_get_length(input) > 0)
      res = decode_websocket(client);
  } else {
    char* line;
    size_t len;
    int res = 0;
    while (res == 0 && (line = evbuffer_readln(input, &len, EVBUFFER_EOL_CRLF))) {
      res = process_line(client, line, len);
      free(line);
    };
  }
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
    if (client->timer) /* Why aren't you checking for NULL libevent?... */
      event_free(client->timer);
    free_websocket(client->websocket);
    free(client);
    bufferevent_free(bev);
  }
};

void client_disconnect_after_write(struct bufferevent* bev, void* context) {
  struct evbuffer* output = bufferevent_get_output(bev);
  if (evbuffer_get_length(output) == 0)
    client_eventcb(bev, BEV_ERROR, context);
};