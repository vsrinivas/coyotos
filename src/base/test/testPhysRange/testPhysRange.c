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

#include <idl/coyotos/AddressSpace.h>
#include <idl/coyotos/Range.h>
#include <idl/coyotos/Process.h>
#include <coyotos/runtime.h>

#define CR_RANGE	CR_APP(0)
#define CR_ADDRSPACE	CR_APP(1)
#define CR_TMP		CR_APP(2)

#define FAIL (void)(*(uint32_t *)0 = __LINE__)
int
main(int argc, char *argv[])
{
  if (!coyotos_Process_getSlot(CR_SELF, coyotos_Process_cslot_addrSpace,
			       CR_ADDRSPACE))
    FAIL;
  if (!coyotos_Range_getCap(CR_RANGE,
			    0, coyotos_Range_obType_otPage, CR_TMP))
    FAIL;
  if (!coyotos_AddressSpace_setSlot(CR_ADDRSPACE, 15, CR_TMP))
    FAIL;

  uint64_t oldval = *(uint64_t *)(15 * COYOTOS_PAGE_SIZE);

  if (!coyotos_Range_getCap(CR_RANGE,
			    coyotos_Range_physOidStart +
			    0x1a3000/COYOTOS_PAGE_SIZE,
			    coyotos_Range_obType_otPage, 
			    CR_TMP))
    FAIL;
  if (!coyotos_AddressSpace_setSlot(CR_ADDRSPACE, 14, CR_TMP))
    FAIL;

  uint64_t newval = *(uint64_t *)(15 * COYOTOS_PAGE_SIZE);

  if (oldval != newval)
    *(uint32_t *)4 = 0x4;

  uint64_t page0val = *(uint64_t *)(14 * COYOTOS_PAGE_SIZE);

  if (page0val != 0)
    *(uint32_t *)4 = 0x8;

  return 0;
}

