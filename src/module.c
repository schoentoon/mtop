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

#include "module.h"

#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

struct module* new_module(char* filename) {
  void* handle = dlopen(filename, RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "%s\n", dlerror());
    return NULL;
  }
  mod_name_function* name = dlsym(handle, "getName");
  if (!name) {
    fprintf(stderr, "%s\n", dlerror());
    return NULL;
  }
  struct module* module = malloc(sizeof(struct module));
  memset(module, 0, sizeof(struct module));
  module->handle = handle;
  module->name = strdup(name());
  return module;
};