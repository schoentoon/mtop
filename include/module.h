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

#ifndef _MODULE_H
#define _MODULE_H

#include <time.h>
#include <stddef.h>

typedef enum {
  FLOAT,
  FLOAT_RANGE
} module_type;

char* module_type_to_string(module_type type);

struct module {
  void* handle;
  void* context;
  char* name;
  module_type type;
  void* module_data;
  size_t array_length;
  void* update_function;
  unsigned char max_interval;
  time_t last_update;
  struct module* next;
};

typedef size_t mod_get_array_length(void* context);

/* FLOAT */
typedef float mod_get_float(void* context, size_t item);

/* FLOAT_RANGE includes mod_get_float(void*) */

typedef float mod_get_max_float(void* context, size_t item);
typedef float mod_get_min_float(void* context, size_t item);

struct float_range_data {
  float current_float;
  float max_value;
  float min_value;
  mod_get_min_float* mod_min_func;
  mod_get_max_float* mod_max_func;
};

typedef void* mod_create_context(char* name);
typedef void mod_free_context(void* context);
typedef char* mod_name_function();
typedef module_type mod_type_function(void* context);
typedef char** mod_list_aliases();
typedef unsigned char mod_max_interval(void* context);
typedef char* mod_item_name(void* context, size_t item);

struct module* new_module(char* filename, char* alias);

void free_module(struct module* module);

struct client;

size_t update_value(struct module* module, char* buf, size_t buf_size, struct client* client);

size_t print_value(struct module* module, char* buf, size_t buf_size, struct client* client);

size_t get_module_item_names(struct module* module, char* buf, size_t buf_size);

#endif //_MODULE_H