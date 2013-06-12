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

#include <stddef.h>

typedef enum {
  FLOAT
} module_type;

struct module {
  void* handle;
  void* context;
  char* name;
  module_type type;
  void* module_data;
  void* update_function;
  struct module* next;
};

typedef void* mod_create_context();
typedef void mod_free_context(void* context);
typedef char* mod_name_function();
typedef module_type mod_type_function(void* context);

typedef float mod_get_float(void* context);

struct module* new_module(char* filename);

void free_module(struct module* module);

size_t update_value(struct module* module, char* buf, size_t buf_size);

#endif //_MODULE_H