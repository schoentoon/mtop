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

#include "websocket.h"

#include "sha1.h"
#include "debug.h"
#include "client.h"
#include "base64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <event2/event.h>
#include <event2/buffer.h>

struct websocket* new_websocket() {
  struct websocket* output = malloc(sizeof(struct websocket));
  memset(output, 0, sizeof(struct websocket));
  return output;
};

void free_websocket(struct websocket* websocket) {
  if (websocket) {
    free(websocket->key);
    free(websocket);
  };
};

int handle_handshake(struct client* client, char* line, size_t len) {
  char buf[1025];
  int number;
  if (!client->websocket) {
    int major, minor;
    if (sscanf(line, "%32s / HTTP/%d.%d", buf, &major, &minor) == 3
      || sscanf(line, "%32s /%*s HTTP/%d.%d", buf, &major, &minor) == 3) {
      client->websocket = new_websocket();
      if (major >= 1 && minor >= 1) {
        client->websocket->is_http_1_1 = 1;
        if (strcasecmp(buf, "GET") == 0)
          client->websocket->is_get = 1;
      }
    }
  } else if (sscanf(line, "Upgrade: %1024s", buf) == 1 && strstr(buf, "websocket"))
    client->websocket->has_upgrade = 1;
  else if (sscanf(line, "Connection: %1024s", buf) == 1 && strstr(buf, "Upgrade"))
    client->websocket->connection_is_upgrade = 1;
  else if (sscanf(line, "Sec-WebSocket-Key: %64s", buf) == 1)
    client->websocket->key = strdup(buf);
  else if (sscanf(line, "Sec-WebSocket-Version: %d", &number) == 1 && number == 13)
    client->websocket->correct_version = 1;
  else if (len == 0) {
    if (client->websocket->has_upgrade
      && client->websocket->connection_is_upgrade
      && client->websocket->correct_version
      && client->websocket->key
      && client->websocket->is_get
      && client->websocket->is_http_1_1) {
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
      free(client->websocket->key);
      client->websocket->key = NULL;
    } else {
      struct evbuffer* output = bufferevent_get_output(client->bev);
      evbuffer_add_printf(output, "HTTP/1.1 404 This is a WebSocket endpoint\r\n"
                                  "Content-Length: 29\r\n\r\n"
                                  "This is a WebSocket endpoint.");
      bufferevent_setcb(client->bev, client_readcb, client_disconnect_after_write, client_eventcb, client);
      bufferevent_enable(client->bev, EV_READ|EV_WRITE);
    }
  } else
    return -1;
  return client->websocket != 0;
};

int decode_websocket(struct client* client) {
  struct evbuffer* input = bufferevent_get_input(client->bev);
  if (client->websocket->length_code == 0 && evbuffer_get_length(input) >= 2) {
    unsigned char header[2];
    bufferevent_read(client->bev, header, sizeof(header));
    if (header[0] == 136) { /* Disconnected */
      client_eventcb(client->bev, BEV_FINISHED, client);
      return 1;
    } else if (header[0] == 129)
      client->websocket->length_code = header[1] & 127;
    else
      DEBUG(255, "What the hell, header[0] = %d, header[1] = %d", header[0], header[1]);
  }
  if (client->websocket->length_code && client->websocket->length == 0) {
    if (client->websocket->length_code <= 125)
      client->websocket->length = client->websocket->length_code;
    else if (client->websocket->length_code == 126) {
      unsigned char lenbuf[2];
      bufferevent_read(client->bev, lenbuf, sizeof(lenbuf));
      client->websocket->length |= lenbuf[0];
      client->websocket->length <<= 8;
      client->websocket->length |= lenbuf[1];
    }
    DEBUG(255, "length = %d", client->websocket->length);
  }
  if (client->websocket->has_mask == 0 && evbuffer_get_length(input) >= 4) {
    bufferevent_read(client->bev, client->websocket->mask, sizeof(client->websocket->mask));
    client->websocket->has_mask = 1;
  }
  if (client->websocket->has_mask && evbuffer_get_length(input) >= client->websocket->length) {
    char data[client->websocket->length + 1];
    bufferevent_read(client->bev, data, client->websocket->length);
    unsigned int i;
    for (i = 0; i < client->websocket->length; i++)
      data[i] = (unsigned char) data[i] ^ client->websocket->mask[i % 4];
    data[i] = '\0';
    int res = process_line(client, data, client->websocket->length);
    client->websocket->length_code = 0;
    client->websocket->length = 0;
    client->websocket->has_mask = 0;
    return res;
  }
  return 1;
};

void encode_and_send_websocket(struct client* client, char* line, size_t length) {
  unsigned char frame[length + 11];
  bzero(frame, sizeof(frame));
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
    frame[data_start_index + i] = line[i];
  struct evbuffer* output = bufferevent_get_output(client->bev);
  evbuffer_add(output, frame, data_start_index + length);
};