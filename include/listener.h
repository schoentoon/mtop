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

#ifndef _LISTENER_H
#define _LISTENER_H

#include "config.h"

#include <string.h>

#include <event2/event.h>

struct listener {
  struct sockaddr_in* address;
  struct evconnlistener* listener;
  struct listener* next;
};

struct listener* new_listener(char* input);

int initListener(struct event_base* base, struct listener* listener);

#endif //_LISTENER_H