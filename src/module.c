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

#include "client.h"
#include "debug.h"

#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

struct module* new_module(char* filename, char* alias) {
  DEBUG(100, "Loading %s with alias %s", filename, alias);
  void* handle = dlopen(filename, RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "%s\n", dlerror());
    return NULL;
  }
  mod_name_function* name = dlsym(handle, "getName");
  if (!name) {
    fprintf(stderr, "%s\n", dlerror());
    dlclose(handle);
    return NULL;
  }
  mod_type_function* type = dlsym(handle, "getType");
  if (!type) {
    fprintf(stderr, "%s\n", dlerror());
    dlclose(handle);
    return NULL;
  }
  struct module* module = malloc(sizeof(struct module));
  memset(module, 0, sizeof(struct module));
  module->handle = handle;
  if (alias)
    module->name = strdup(alias);
  else
    module->name = strdup(name());
  mod_create_context* create_context = dlsym(handle, "createContext");
  if (create_context) {
    module->context = create_context(module->name);
    mod_free_context* free_context = dlsym(handle, "freeContext");
    if (!free_context)
      fprintf(stderr, "WARNING, you seem to have a createContext() function but no freeContext() function, you're possibly leaking memory.\n");
  }
  module->type = type(module->context);
  switch (module->type) {
  case FLOAT: {
    mod_get_float* mod_float = dlsym(handle, "getFloat");
    if (!mod_float) {
      fprintf(stderr, "%s\n", dlerror());
      free_module(module);
      return NULL;
    }
    module->update_function = mod_float;
    module->module_data = malloc(sizeof(float));
    break;
  }
  };
  if (!alias) {
    mod_list_aliases* list_aliases_func = dlsym(handle, "listAliases");
    if (list_aliases_func) {
      char** aliases = list_aliases_func();
      if (!aliases)
        return module;
      struct module* first = module;
      int i = 0;
      while (aliases[i]) {
        module->next = new_module(filename, aliases[i]);
        module = module->next;
        free(aliases[i++]);
      };
      free(aliases);
      return first;
    }
  }
  return module;
};

void free_module(struct module* module) {
  if (module) {
    if (module->context) {
      mod_free_context* free_context = dlsym(module->handle, "freeContext");
      free_context(module->context);
    }
    dlclose(module->handle);
    free(module->name);
    switch (module->type) {
    case FLOAT:
      free(module->module_data);
      break;
    };
    free(module);
  }
};

size_t update_value(struct module* module, char* buf, size_t buf_size, struct client* client) {
  switch (module->type) {
  case FLOAT: {
    mod_get_float* mod_float = (mod_get_float*) module->update_function;
    float tmp = mod_float(module->context);
    *((float*) module->module_data) = tmp;
    return snprintf(buf, buf_size, "%.*f", client->precision, tmp);
  };
  };
};