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

#include <coyotos/machine/pagesize.h>
#include <coyotos/coytypes.h>
#include <coyotos/syscall.h>

#include "coyotos/capability_stack.h"

#include <inttypes.h>

static uintptr_t cur = 0;
static uintptr_t max = COYOTOS_PAGE_SIZE;


bool 
capability_push(caploc_t cap)
{
  if (cur >= max)
    return false;  // can't push

  cap_copy(ADDR_CAPLOC(cur), cap);
  cur += COYOTOS_CAPABILITY_SIZE;

  return true;
}

void
capability_pop(caploc_t cap)
{
  if (cur == 0)
    *(uint32_t *)1 = 0;   /* crash */

  cur -= COYOTOS_CAPABILITY_SIZE;
  cap_copy(cap, ADDR_CAPLOC(cur));
}

bool
capability_canPush(uint32_t n)
{
  return ((max - cur) / COYOTOS_CAPABILITY_SIZE) >= n;
}
