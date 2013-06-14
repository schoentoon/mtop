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
#include <stdlib.h>
#include <string.h>

#define PROC_STAT "/proc/stat"

char* getName() {
  return "cpu";
};

int count_cpus() {
  FILE* stat = fopen(PROC_STAT, "r");
  if (!stat)
    return -1;
  int cpu_count = 0;
  while (fgetc(stat) != '\n');
  while (fscanf(stat, "cpu%d", &cpu_count) == 1) 
    while (fgetc(stat) != '\n');
  fclose(stat);
  return cpu_count + 1;
};

char** listAliases() {
  int cpu_count = count_cpus();
  if (cpu_count < 0)
    return NULL;
  char** output = malloc(sizeof(char*) * (cpu_count + 1));
  memset(output, 0, sizeof(char*) * (cpu_count + 1));
  int i;
  char buf[BUFSIZ];
  for (i = 0; i < cpu_count; i++) {
    if (snprintf(buf, sizeof(buf), "cpu%d", i))
      output[i] = strdup(buf);
  };
  return output;
};

module_type getType(void* context) {
  return FLOAT;
};

float getFloat(void* context) {
  return 0.0;
};