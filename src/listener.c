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

#include "listener.h"

#include "debug.h"
#include "client.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <event2/bufferevent.h>

struct listener* new_listener(char* input) {
  struct listener* output = malloc(sizeof(struct listener));
  if (!output)
    return NULL;
  memset(output, 0, sizeof(struct listener));
  output->address = malloc(sizeof(struct sockaddr_in));
  if (!output->address) {
    free(output);
    return NULL;
  }
  memset(output->address, 0, sizeof(struct sockaddr_in));
  char ip[BUFSIZ];
  in_port_t port;
  if (sscanf(input, "%[0-9.]:%hd", ip, &port) == 2) {
    output->address->sin_family = AF_INET;
    output->address->sin_port = htons(port);
    inet_pton(AF_INET, ip, &output->address->sin_addr);
  } else if (sscanf(input, "%hd", &port) == 1) {
    output->address->sin_family = AF_INET;
    output->address->sin_port = htons(port);
  } else {
    free(output->address);
    free(output);
    return NULL;
  }
  return output;
};

static void listener_callback(struct evconnlistener* listener, evutil_socket_t fd
                             ,struct sockaddr* sa, int socklen, void* context);

int initListener(struct event_base* base, struct listener* listener) {
  listener->listener = evconnlistener_new_bind(base, listener_callback, base
                                              ,LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1
                                              ,(struct sockaddr*) listener->address, sizeof(struct sockaddr_in));
  if (!listener->listener) {
    char ipbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &listener->address->sin_addr, ipbuf, sizeof(ipbuf));
    fprintf(stderr, "Error '%s' on %s:%d.\n", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()), ipbuf, ntohs(listener->address->sin_port));
    return 1;
  }
  return 0;
};

static void listener_callback(struct evconnlistener* listener, evutil_socket_t fd
                             ,struct sockaddr* sa, int socklen, void* context) {
  struct event_base* base = context;
  struct bufferevent* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
  if (!bev) {
    event_base_loopbreak(base);
    return;
  }
  struct client* client = new_client();
  bufferevent_setcb(bev, client_readcb, NULL, client_eventcb, client);
  bufferevent_enable(bev, EV_READ);
};