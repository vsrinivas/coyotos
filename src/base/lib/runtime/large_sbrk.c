/*
 * Copyright (C) 2007, The EROS Group, LLC.
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

/* Simulated sbrk */

#include <errno.h>
#include <stdint.h>
#include <sys/types.h>

#include <coyotos/runtime.h>
#include <idl/coyotos/Cap.h>
#include <idl/coyotos/ElfSpace.h>

extern unsigned _end;		/* lie */
static caddr_t heap_ptr = (caddr_t) &_end;

caddr_t
sbrk(intptr_t nbytes)
{
  static int type = 0;
  if (type == 0) {
    coyotos_Cap_AllegedType ty = 0;
    if (coyotos_Cap_getType(CR_ADDRHANDLER, &ty) && ty == IKT_coyotos_ElfSpace)
      type = 2;
    else
      type = 1;
  }
  caddr_t base = heap_ptr;
  caddr_t new_heap = heap_ptr + nbytes;

  if (type == 2) {
    if (!coyotos_ElfSpace_setBreak(CR_ADDRHANDLER, (uintptr_t)new_heap)) {
      errno = ENOMEM;
      return (caddr_t)-1;
    }
  }

  heap_ptr = new_heap;
  return base;
}
