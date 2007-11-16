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

#include <idl/coyotos/Constructor.h>
#include <idl/coyotos/Process.h>
#include <idl/coyotos/SpaceBank.h>
#include <idl/coyotos/IoStream.h>
#include <coyotos/runtime.h>
#include <coyotos/kprintf.h>
#include <string.h>

#define CR_CTOR         CR_APP(0)
#define CR_LOG          CR_APP(1)
#define CR_SCHED        CR_APP(2)
#define CR_YIELD        CR_APP(3)
#define CR_SUBBANK      CR_APP(4)

#define unless(x) if (!(x))

#define FAIL (void)(*(uint32_t *)0 = __LINE__)

#define CHECK(expr)							\
  do {									\
    if (!(expr))							\
      kprintf(CR_LOG, "%s:%d: FAIL %s\n", __FILE__, __LINE__, #expr);	\
  } while (0)

      
int
main(int argc, char *argv[])
{
  coyotos_Cap_AllegedType at = 0;
  uint32_t len;

  char buf[coyotos_IoStream_bufLimit];
  coyotos_IoStream_chString s = { coyotos_IoStream_bufLimit, 0, buf };

  memset(buf, 0, coyotos_IoStream_bufLimit);

  CHECK (coyotos_Process_getSlot(CR_SELF, coyotos_Process_cslot_schedule,
				 CR_SCHED));

  /* Create sub-bank for the TextConsole instance */
  CHECK (coyotos_SpaceBank_createChild(CR_SPACEBANK, CR_SUBBANK));

  CHECK (coyotos_Constructor_create(CR_CTOR, CR_SUBBANK, CR_SCHED, CR_RUNTIME, CR_YIELD));

  CHECK(coyotos_Cap_getType(CR_YIELD, &at));
  CHECK((at == IKT_coyotos_IoStream));

  CHECK(coyotos_IoStream_read(CR_YIELD, coyotos_IoStream_bufLimit, &s));
  CHECK(s.len == 0);

  CHECK(coyotos_IoStream_read(CR_YIELD, coyotos_IoStream_bufLimit+1, &s) 
	== false);
  CHECK(__IDL_Env->errCode == RC_coyotos_Cap_RequestError);


  s.len = 4096;
  CHECK(coyotos_IoStream_write(CR_YIELD, s, &len));
  CHECK(len == coyotos_IoStream_bufLimit);

  kprintf(CR_LOG, "PASS\n");

  for(;;) ;
  return 0;
}

