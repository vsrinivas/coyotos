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

#include <kerninc/capability.h>
#include <kerninc/InvParam.h>
#include <kerninc/Process.h>
#include <kerninc/util.h>
#include <coyotos/syscall.h>
#include <hal/syscall.h>
#include <idl/coyotos/AddressSpace.h>

extern void cap_Memory(InvParam_t* iParam);

void
cap_AddressSpace(InvParam_t *iParam)
{

  /* It turns out that all of the operations defined in this interface
     are highly personalized to the specific object, so there is no point
     doing a common implementation here. Look in the individual object
     implementations for the subset of implemented opcodes. */

  cap_Memory(iParam);
}
