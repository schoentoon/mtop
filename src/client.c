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

#include <stdlib.h>

#include <event2/buffer.h>

struct client* new_client() {
  return malloc(sizeof(struct client));
};

void client_readcb(struct bufferevent* bev, void* context) {
  struct evbuffer* input = bufferevent_get_input(bev);
  size_t len;
  char* line = evbuffer_readln(input, &len, EVBUFFER_EOL_ANY);
  while (line) {
    DEBUG(255, "Raw line: %s", line);
    free(line);
    line = evbuffer_readln(input, &len, EVBUFFER_EOL_ANY);
  };
};

void client_eventcb(struct bufferevent* bev, short events, void* context) {
  if (!(events & BEV_EVENT_CONNECTED)) {
    struct client* client = context;
    free(client);
  }
};