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

#define PROC_LOADAVG "/proc/loadavg"

#define SHORTTERM 0
#define MIDTERM 1
#define LONGTERM 2

char* getName() {
  return "load";
};

char* getItemName(void* context, size_t item) {
  switch (item) {
  case SHORTTERM:
    return "shortterm";
  case MIDTERM:
    return "midterm";
  case LONGTERM:
    return "longterm";
  };
  return NULL;
};

module_type getType(void* context) {
  return FLOAT;
};

size_t getArrayLength(void* context) {
  return 3;
};

float getFloat(void* context, size_t i) {
  FILE* f = fopen(PROC_LOADAVG, "r");
  float shortterm, midterm, longterm;
  if (fscanf(f, "%f %f %f", &shortterm, &midterm, &longterm) == 3) {
    fclose(f);
    switch (i) {
    case SHORTTERM:
      return shortterm;
    case MIDTERM:
      return midterm;
    case LONGTERM:
      return longterm;
    }
  }
  fclose(f);
  return 0.0;
};