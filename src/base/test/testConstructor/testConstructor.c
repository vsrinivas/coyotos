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

#include <idl/coyotos/Builder.h>
#include <idl/coyotos/KernLog.h>

#include <coyotos/runtime.h>

#include "testConstructor.h"

#include <string.h>

void
kernlog(const char *message)
{
  size_t len = strlen(message);
  
  if (len > 255) 
    len = 255;
  
  coyotos_KernLog_logString str = { 
    .max = 256, .len = len, .data = (char *)message
  };
  
  (void) coyotos_KernLog_log(testConstructor_REG_KERNLOG, str);
}

int
main(int argc, char *argv[])
{
  uint64_t type = 0;

  if (!coyotos_Constructor_create(testConstructor_REG_METACON,
				  testConstructor_REG_BANK,
				  testConstructor_REG_SCHED,
				  testConstructor_REG_RUNTIME,
				  testConstructor_REG_TMP))
    kernlog("create failed\n");
  else if (!coyotos_Cap_getType(testConstructor_REG_TMP, &type))
    kernlog("getType failed\n");

  else if (type != IKT_coyotos_Builder)
    kernlog("getType bad result\n");

  else
    kernlog("constructor IKT successful\n");
  
  *(uint64_t *)0 = 0;
  return 0;
}

