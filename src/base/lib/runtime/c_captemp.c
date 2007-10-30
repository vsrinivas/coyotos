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

/** @file */

#include <coyotos/machine/pagesize.h>
#include <coyotos/coytypes.h>

#include "coyotos/captemp.h"

#include <inttypes.h>

static uintptr_t cur = 0;
static uintptr_t max = COYOTOS_PAGE_SIZE;

caploc_t
captemp_alloc(void)
{
  if (cur >= max) {
    *(uint32_t *)1 = 0;  /** @bug this should be a user-defined exception */
  }

  caploc_t ret = ADDR_CAPLOC(cur);
  cur += COYOTOS_CAPABILITY_SIZE;

  return ret;
}

void
captemp_release(caploc_t cap)
{
  uintptr_t new_cur = cur - COYOTOS_CAPABILITY_SIZE;

  if (cur == 0 || cap.fld.ty != CAPLOC_TY_MEM || (cap.fld.loc << 1) != new_cur)
    *(uint32_t *)1 = 0;   /* crash */

  cur = new_cur;
}
