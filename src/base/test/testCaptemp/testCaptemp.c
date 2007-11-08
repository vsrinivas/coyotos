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

#include <string.h>
#include <stdlib.h>

#include <coyotos/captemp.h>
#include <coyotos/runtime.h>
#include <idl/coyotos/SpaceBank.h>
#include <coyotos/kprintf.h>

int
main(int argc, char *argv[])
{
	caploc_t cap = captemp_alloc();

	kprintf(CR_APP0, "Attempting allocation\n");
	if (!coyotos_SpaceBank_alloc(CR_SPACEBANK,
				     coyotos_Range_obType_otGPT,
				     coyotos_Range_obType_otInvalid,
				     coyotos_Range_obType_otInvalid,
				     cap, CR_NULL, CR_NULL)) {
	   kprintf(CR_APP0, "Alloc attempt failed\n");
	   return 0;
	}

	kprintf(CR_APP0, "Allocation succeeded.\n");

        captemp_release(cap);
	return 0;
}

