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

#ifndef _CONFIG_H
#define _CONFIG_H

#include "module.h"
#include "listener.h"

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>

struct config {
  struct listener* listeners;
  struct module* modules;
};

int parse_config(char* config_file);

int dispatch_config(struct event_base* event_base);

int send_loaded_modules_info(struct bufferevent* bev);

#endif //_CONFIG_H