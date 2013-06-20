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

#include "debug.h"

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

struct CPUData {
  int cpu;
  unsigned long long loadTime;
  unsigned long long totalTime;
};

float getFloat(void* context);

void* createContext(char* name) {
  struct CPUData* output = malloc(sizeof(struct CPUData));
  memset(output, 0, sizeof(struct CPUData));
  if (sscanf(name, "cpu%d", &output->cpu) != 1)
    output->cpu = -1;
  getFloat((void*) output); /* Just to fill in load and total */
  return output;
};

void freeContext(void* context) {
  free(context);
};

module_type getType(void* context) {
  return FLOAT;
};

float getFloat(void* context) {
  struct CPUData* cpudata = context;
  FILE* stat = fopen(PROC_STAT, "r");
  unsigned long long user, nice, system, idle;
  if (cpudata->cpu == -1) {
    if (fscanf(stat, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle) != 4)
      return -1.0;
  } else {
    int cpu;
    int cpu_we_want = cpudata->cpu;
    while (fgetc(stat) != '\n');
    while (fscanf(stat, "cpu%d %llu %llu %llu %llu", &cpu, &user, &nice, &system, &idle) == 5 && cpu != cpu_we_want)
      while (fgetc(stat) != '\n');
  }
  fclose(stat);
  unsigned long long load = user + nice + system;
  unsigned long long total = load + idle;
  float output = ((float) ((load - cpudata->loadTime) * 100)) / ((float) (total - cpudata->totalTime));
  cpudata->loadTime = load;
  cpudata->totalTime = total;
  return output;
};