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

#include "debug.h"
#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void decode_websocket(struct bufferevent* bev, struct client* client) {
  struct evbuffer* input = bufferevent_get_input(bev);
  if (client->websocket->length_code == 0 && evbuffer_get_length(input) >= 2) {
    unsigned char header[2];
    bufferevent_read(bev, header, sizeof(header));
    if (header[0] == 136) /* Disconnected */
      client_eventcb(bev, BEV_FINISHED, client);
    else if (header[0] == 129)
      client->websocket->length_code = header[1] & 127;
    else
      DEBUG(255, "What the hell, header[0] = %d, header[1] = %d", header[0], header[1]);
  }
  if (client->websocket->length_code && client->websocket->length == 0) {
    if (client->websocket->length_code <= 125)
      client->websocket->length = client->websocket->length_code;
    else if (client->websocket->length_code == 126) {
      unsigned char lenbuf[2];
      bufferevent_read(bev, lenbuf, sizeof(lenbuf));
      client->websocket->length |= lenbuf[0];
      client->websocket->length <<= 8;
      client->websocket->length |= lenbuf[1];
    }
    DEBUG(255, "length = %d", client->websocket->length);
  }
  if (client->websocket->has_mask == 0 && evbuffer_get_length(input) >= 4) {
    bufferevent_read(bev, client->websocket->mask, sizeof(client->websocket->mask));
    client->websocket->has_mask = 1;
  }
  if (client->websocket->has_mask && evbuffer_get_length(input) >= client->websocket->length) {
    unsigned char data[client->websocket->length + 1];
    bufferevent_read(bev, data, client->websocket->length);
    unsigned int i;
    char buf[client->websocket->length + 1];
    for (i = 0; i < client->websocket->length; i++)
      buf[i] = data[i] ^ client->websocket->mask[i % 4];
    buf[i] = '\0';
    process_line(client, buf, client->websocket->length);
    client->websocket->length_code = 0;
    client->websocket->length = 0;
    client->websocket->has_mask = 0;
    if (evbuffer_get_length(input) > 0)
      decode_websocket(bev, client);
  }
};