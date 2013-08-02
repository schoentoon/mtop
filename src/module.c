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

char* module_type_to_string(module_type type) {
  switch (type) {
  case FLOAT:
    return "FLOAT";
  case FLOAT_RANGE:
    return "FLOAT_RANGE";
  }
  return ""; /* gcc is stupid after all. */
}

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
      fprintf(stderr, "WARNING, you seem to have a createContext() function but no freeContext() function, you're possibly leaking memory.\n\t%s\n", dlerror());
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
    mod_get_array_length* mod_array_length_func = dlsym(handle, "getArrayLength");
    if (mod_array_length_func)
      module->array_length = mod_array_length_func(module->context);
    else
      module->array_length = 1;
    module->update_function = mod_float;
    module->module_data = calloc(module->array_length + 1, sizeof(float));
    size_t i;
    for (i = 0; i < module->array_length; i++)
      ((float**) module->module_data)[i] = malloc(sizeof(float));
    ((float**) module->module_data)[module->array_length] = NULL;
    break;
  }
  case FLOAT_RANGE: {
    mod_get_float* mod_float = dlsym(handle, "getFloat");
    if (!mod_float) {
      fprintf(stderr, "%s\n", dlerror());
      free_module(module);
      return NULL;
    }
    module->update_function = mod_float;
    mod_get_max_float* mod_max_float = dlsym(handle, "getMaxFloat");
    if (!mod_max_float) {
      fprintf(stderr, "%s\n", dlerror());
      free_module(module);
      return NULL;
    }
    mod_get_min_float* mod_min_float = dlsym(handle, "getMinFloat");
    if (!mod_min_float) {
      fprintf(stderr, "%s\n", dlerror());
      free_module(module);
      return NULL;
    }
    mod_get_array_length* mod_array_length_func = dlsym(handle, "getArrayLength");
    if (mod_array_length_func)
      module->array_length = mod_array_length_func(module->context);
    else
      module->array_length = 1;
    module->module_data = calloc(module->array_length + 1, sizeof(struct float_range_data));
    ((struct float_range_data**) module->module_data)[module->array_length] = NULL;
    size_t i;
    for (i = 0; i < module->array_length; i++) {
      struct float_range_data* data = malloc(sizeof(struct float_range_data));
      ((struct float_range_data**) module->module_data)[i] = data;
      data->mod_max_func = mod_max_float;
      data->mod_min_func = mod_min_float;
      data->max_value = mod_max_float(module->context, i);
      data->min_value = mod_min_float(module->context, i);
    }
    break;
  }
  };
  mod_max_interval* max_interval_func = dlsym(handle, "maxInterval");
  if (max_interval_func)
    module->max_interval = max_interval_func(module->context);
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
      if (free_context)
        free_context(module->context);
    }
    dlclose(module->handle);
    free(module->name);
    switch (module->type) {
    case FLOAT:
    case FLOAT_RANGE:
      free(module->module_data);
      break;
    };
    free(module);
  }
};

size_t print_value(struct module* module, char* buf, size_t buf_size, struct client* client) {
  char* s = buf;
  char* end = buf + buf_size;
  switch (module->type) {
  case FLOAT: {
    size_t i;
    for (i = 0; i < module->array_length; i++) {
      s += snprintf(s, end - s, "%.*f", client->precision, *((float**) module->module_data)[0]);
      if (i != (module->array_length - 1))
        s += snprintf(s, end - s, ", ");
    }
    return s - buf;
  }
  case FLOAT_RANGE: {
    size_t i;
    for (i = 0; i < module->array_length; i++) {
      struct float_range_data* data = ((struct float_range_data**) module->module_data)[i];
      s += snprintf(s, end - s, "%.*f/%.*f/%.*f"
                   ,client->precision, data->min_value
                   ,client->precision, data->current_float
                   ,client->precision, data->max_value);
      if (i != (module->array_length - 1))
        s += snprintf(s, end - s, ", ");
    }
    return s - buf;
  }
  default:
    return 0;
  }
};

size_t update_value(struct module* module, char* buf, size_t buf_size, struct client* client) {
  if (module->max_interval) {
    time_t now = time(NULL);
    if (now - module->last_update <= module->max_interval)
      return print_value(module, buf, buf_size, client);
  }
  switch (module->type) {
  case FLOAT: {
    mod_get_float* mod_float = (mod_get_float*) module->update_function;
    size_t i;
    for (i = 0; i < module->array_length; i++) {
      float new_value = mod_float(module->context, i);
      *(((float**) module->module_data)[i]) = new_value;
    }
    return print_value(module, buf, buf_size, client);
  };
  case FLOAT_RANGE: {
    mod_get_float* mod_float = (mod_get_float*) module->update_function;
    size_t i;
    for (i = 0; i < module->array_length; i++) {
      struct float_range_data* data = ((struct float_range_data**) module->module_data)[i];
      data->min_value = data->mod_min_func(module->context, i);
      data->max_value = data->mod_max_func(module->context, i);
      float new_value = mod_float(module->context, i);
      if (new_value >= data->min_value && new_value <= data->max_value)
        data->current_float = new_value;
    }
    return print_value(module, buf, buf_size, client);
  }
  };
  return 0; /* gcc is stupid after all. */
};