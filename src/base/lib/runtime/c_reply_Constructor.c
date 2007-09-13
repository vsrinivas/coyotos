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

/* @file reply to Constructor call */

#include <string.h>

#include <coyotos/syscall.h>
#include <idl/coyotos/Constructor.h>

#include "coyotos/reply_Constructor.h"

void
reply_coyotos_Constructor_create(caploc_t ret, caploc_t replyEntry)
{
  _INV_coyotos_Constructor_create p;
  p.server_out._icw = IPW0_MAKE_NR(sc_InvokeCap)
    |IPW0_SP|IPW0_MAKE_LDW(0)|IPW0_SC|IPW0_MAKE_LSC(0);
  p.server_out._invCap = ret;
  p.server_out._retVal = replyEntry;
  p.server_out._sndLen = 0;

  (void) invoke_capability(&p.pb);
}
