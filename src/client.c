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

#include "sha1.h"
#include "debug.h"
#include "base64.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <event2/buffer.h>

struct client* new_client() {
  struct client* client = malloc(sizeof(struct client));
  memset(client, 0, sizeof(struct client));
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

void process_line(struct client* client, char* line, size_t len) {
  DEBUG(255, "Raw line: %s", line);
  char buf[65];
  int number;
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
        do {
          if (lm->module == em->module) {
            client_send_data(client, "ALREADY LOADED %s WITH ID %d", em->module->name, em->id);
            free(em);
            lm = NULL;
            break;
          }
          em->id = lm->id + 1;
          lm = lm->next;
        } while (lm);
        if (lm) {
          lm->next = em;
          client_send_data(client, "LOADED %s WITH ID %d", buf, em->id);
        }
      }
    } else
      client_send_data(client, "UNABLE TO LOAD %s\n", buf);
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
      if (update_value(module, databuf, sizeof(databuf)))
        client_send_data(client, "%s: %s", module->name, databuf);
    };
  } else if (sscanf(line, "INTERVAL %ld:%ld", &tv.tv_sec, &tv.tv_usec) == 2 || sscanf(line, "INTERVAL %ld", &tv.tv_sec) == 1) {
    if (client->timer)
      event_free(client->timer);
    client->timer = event_new(bufferevent_get_base(client->bev), -1, EV_PERSIST, client_timer, client);
    event_add(client->timer, &tv);
  } else {
    char bigbuf[1025];
    if (!client->websocket) {
      int major, minor;
      if (sscanf(line, "GET / HTTP/%d.%d", &major, &minor) == 2) {
        if (major >= 1 && minor >= 1)
          client->websocket = new_websocket();
        else {
          struct evbuffer* output = bufferevent_get_output(client->bev);
          evbuffer_add_printf(output, "HTTP/%d.%d 500 Must be at least HTTP 1.1\r\n\r\n", major, minor);
        }
      }
    } else if (sscanf(line, "Upgrade: %1024s", bigbuf) == 1 && strstr(bigbuf, "websocket"))
      client->websocket->has_upgrade = 1;
    else if (sscanf(line, "Connection: %1024s", bigbuf) == 1 && strstr(bigbuf, "Upgrade"))
      client->websocket->connection_is_upgrade = 1;
    else if (sscanf(line, "Sec-WebSocket-Key: %64s", buf) == 1)
      client->websocket->key = strdup(buf);
    else if (sscanf(line, "Sec-WebSocket-Version: %d", &number) == 1 && number == 13)
      client->websocket->correct_version = 1;
    else if (len == 0 && client->websocket->has_upgrade && client->websocket->connection_is_upgrade && client->websocket->correct_version) {
      SHA_CTX c;
      unsigned char *base64_encoded;
      unsigned char raw[SHA1_LEN];
      size_t out_len;
      SHA1_Init(&c);
      char keybuf[BUFSIZ];
      snprintf(keybuf, sizeof(keybuf), "%s%s", client->websocket->key, MAGIC_STRING);
      SHA1_Update(&c, keybuf, strlen(keybuf));
      SHA1_Final(raw, &c);
      base64_encoded = base64_encode((unsigned char*) raw, SHA1_LEN, &out_len);
      struct evbuffer* output = bufferevent_get_output(client->bev);
      evbuffer_add_printf(output, "HTTP/1.1 101 Switching Protocols\r\n"
                                  "Upgrade: websocket\r\n"
                                  "Connection: Upgrade\r\n"
                                  "Sec-WebSocket-Accept: %s\r\n\r\n", base64_encoded);
      client->websocket->connected = 1;
    }
  }
};

void client_send_data(struct client* client, char* line, ...) {
  if (client->websocket && client->websocket->connected) {
    char data[BUFSIZ];
    va_list arg;
    va_start(arg, line);
    size_t length = vsnprintf(data, sizeof(data), line, arg);
    va_end(arg);
    unsigned char frame[BUFSIZ];
    int data_start_index;
    frame[0] = 129;
    if (length <= 125) {
      frame[1] = (unsigned char) length;
      data_start_index = 2;
    } else if (length > 125 && length <= 65535) {
      frame[1] = 126;
      frame[2] = (unsigned char) ((length >> 8) & 255);
      frame[3] = (unsigned char) ((length) & 255);
      data_start_index = 4;
    } else {
      frame[1] = 127;
      frame[2] = (unsigned char) ((length >> 56) & 255);
      frame[3] = (unsigned char) ((length >> 48) & 255);
      frame[4] = (unsigned char) ((length >> 40) & 255);
      frame[5] = (unsigned char) ((length >> 32) & 255);
      frame[6] = (unsigned char) ((length >> 24) & 255);
      frame[7] = (unsigned char) ((length >> 16) & 255);
      frame[8] = (unsigned char) ((length >> 8) & 255);
      frame[9] = (unsigned char) ((length) & 255);
      data_start_index = 10;
    }
    int i;
    for (i = 0; i < length; i++)
      frame[data_start_index + i] = data[i];
    struct evbuffer* output = bufferevent_get_output(client->bev);
    evbuffer_add(output, frame, data_start_index + length);
  } else {
    struct evbuffer* output = bufferevent_get_output(client->bev);
    va_list arg;
    va_start(arg, line);
    evbuffer_add_vprintf(output, line, arg);
    va_end(arg);
    evbuffer_add(output, "\n", 2);
  }
};

void client_readcb(struct bufferevent* bev, void* context) {
  struct client* client = context;
  if (client->websocket && client->websocket->connected) {
    char data[BUFSIZ];
    size_t read_bytes = bufferevent_read(bev, &data, sizeof(data));
    if (read_bytes) {
      if (((unsigned char) data[0]) == 136) /* Disconnected */
        client_eventcb(bev, BEV_FINISHED, context);
      else {
        unsigned int length_code = 0;
        unsigned int packet_length = 0;
        int index_first_mask = 0;
        int index_first_data_byte = 0;
        unsigned char mask[4];
        length_code = ((unsigned char) data[1]) & 127;
        if (length_code <= 125) {
          index_first_mask = 2;
          mask[0] = data[2];
          mask[1] = data[3];
          mask[2] = data[4];
          mask[3] = data[5];
        } else if (length_code == 126) {
          index_first_mask = 4;
          mask[0] = data[4];
          mask[1] = data[5];
          mask[2] = data[6];
          mask[3] = data[7];
        } else if (length_code == 127) {
          index_first_mask = 10;
          mask[0] = data[10];
          mask[1] = data[11];
          mask[2] = data[12];
          mask[3] = data[13];
        }
        index_first_data_byte = index_first_mask + 4;
        packet_length = read_bytes - index_first_data_byte;
        int i, j;
        char buf[BUFSIZ];
        for (i = index_first_data_byte, j = 0; i < read_bytes; i++, j++)
          buf[j] = (unsigned char) data[i] ^ mask[j % 4];
        buf[j++] = '\0';
        process_line(client, buf, packet_length);
      }
    }
  } else {
    struct evbuffer* input = bufferevent_get_input(bev);
    char* line;
    size_t len;
    while ((line = evbuffer_readln(input, &len, EVBUFFER_EOL_CRLF))) {
      process_line(client, line, len);
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
    event_free(client->timer);
    free_websocket(client->websocket);
    free(client);
    bufferevent_free(bev);
  }
};