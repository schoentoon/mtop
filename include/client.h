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

#ifndef _CLIENT_H
#define _CLIENT_H

#include "module.h"
#include "websocket.h"

#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>

struct enabled_mod {
  struct module* module;
  uint16_t id;
  struct enabled_mod* next;
};

struct client {
  struct bufferevent* bev;
  struct enabled_mod* mods;
  struct event* timer;
  struct websocket* websocket;
  unsigned char precision;
  unsigned char unknown_command : 2;
};

struct client* new_client();

void client_send_data(struct client* client, char* line, ...);

void client_readcb(struct bufferevent* bev, void* context);

void client_eventcb(struct bufferevent* bev, short events, void* context);

void client_disconnect_after_write(struct bufferevent* bev, void* context);

int process_line(struct client* client, char* line, size_t len);

#endif //_CLIENT_H