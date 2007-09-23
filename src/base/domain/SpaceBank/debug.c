/*
 * Copyright (C) 2007, The EROS Group, LLC
 *
 * This file is part of the Coyotos Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief SpaceBank debugging support file
 */

#include "SpaceBank.h"

#include <idl/coyotos/KernLog.h>
#include <string.h>

static void
log_message(const char *message)
{
  size_t len = strlen(message);
  if (len > 255) 
    len = 255;
  
  coyotos_KernLog_logString str = { 
    .max = 256, .len = len, .data = (char *)message
  };
  
  (void) coyotos_KernLog_log(CR_KERNLOG, str, IDL_E);
}

void
strcat_number(char *string, uint64_t value)
{
#define MAX 30
  char output[MAX+1];
  int idx = MAX;

  output[idx] = 0;

  while (value || idx == MAX) {
    output[--idx] = '0' + (value % 10);
    value /= 10;
  }

  strcat(string, &output[idx]);
}

int
__assert3_fail(const char *file, int lineno, const char *desc, uint64_t val1, uint64_t val2)
{
  char m[256];
  strcpy(m, "SpaceBank: assertion failed: ");
  strcat(m, file);
  strcat(m, ":");
  strcat_number(m, lineno);
  strcat(m, ": (");
  strcat_number(m, val1);
  strcat(m, ") ");
  strcat(m, desc);
  strcat(m, " (");
  strcat_number(m, val2);
  strcat(m, ")\n");
  
  log_message(m);
  *(const char **)0 = desc;
  return 0;
}

int
__assert_fail(const char *file, int lineno, const char *desc)
{
  char m[256];

  strcpy(m, "SpaceBank: assertion failed: ");
  strcat(m, file);
  strcat(m, ":");
  strcat_number(m, lineno);
  strcat(m, ": ");
  strcat(m, desc);
  strcat(m, "\n");
  
  log_message(m);
  *(const char **)0 = desc;
  return 0;
}

