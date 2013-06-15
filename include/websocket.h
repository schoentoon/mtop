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

#ifndef _WEBSOCKET_H
#define _WEBSOCKET_H

#include <event2/bufferevent.h>

#include <sys/types.h>

#define MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

#define MASK_LEN 4

struct websocket {
  char* key;
  unsigned char has_upgrade : 1;
  unsigned char connection_is_upgrade : 1;
  unsigned char correct_version : 1;
  unsigned char connected : 1;

  unsigned char length_code;
  char mask[MASK_LEN];
  unsigned char has_mask : 1;
  size_t length;
};

struct websocket* new_websocket();

void free_websocket(struct websocket* websocket);

struct client;

int handle_handshake(struct client* client, char* line, size_t len);

void decode_websocket(struct bufferevent* bev, struct client* client);

void encode_and_send_websocket(struct client* client, char* line, size_t length);

#endif //_WEBSOCKET_H