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

/** @file Small Space Data initialization */

#include <stddef.h>
#include <inttypes.h>
#include <coyotos/coytypes.h>
#include <coyotos/runtime.h>
#include <idl/coyotos/AddressSpace.h>
#include <idl/coyotos/Range.h>
#include <idl/coyotos/SpaceBank.h>

/* n must be < 4 */
#define CR_UNSTABLE(n)	CR_APP(CRN_LASTAPP_STABLE - CRN_FIRSTAPP + 1 + (n))

#define CR_NEWADDR	CR_UNSTABLE(0) /* new address space */
#define CR_NEWPAGE	CR_UNSTABLE(1) /* available for new page */
#define CR_OLDADDR	CR_UNSTABLE(2) /* old address space */
#define CR_OLDPAGE	CR_UNSTABLE(3) /* old page */

void
__small_data_init(uintptr_t data, uintptr_t end)
{
  size_t cur;
  IDL_Environment _IDL_E = {
    .replyCap = CR_REPLYEPT,
    .epID = 0ULL
  };
  size_t first_slot = data / COYOTOS_PAGE_SIZE;
  size_t last_slot = (end + COYOTOS_PAGE_SIZE - 1) / COYOTOS_PAGE_SIZE;

  for (cur = first_slot; cur < last_slot; cur++) {
    if (!coyotos_AddressSpace_getSlot(CR_OLDADDR, cur, CR_OLDPAGE, &_IDL_E) ||
	!coyotos_SpaceBank_alloc(CR_SPACEBANK,
				 coyotos_Range_obType_otPage,
				 coyotos_Range_obType_otInvalid,
				 coyotos_Range_obType_otInvalid,
				 CR_NEWPAGE,
				 CR_NULL,
				 CR_NULL,
				 &_IDL_E) ||
	!coyotos_AddressSpace_copyFrom(CR_NEWPAGE, CR_OLDPAGE, 
				       CR_NEWPAGE, &_IDL_E) ||
	!coyotos_AddressSpace_setSlot(CR_NEWADDR, cur, CR_NEWPAGE, &_IDL_E))
      goto fail;
  }

  /* install a CapPage in slot 0 for the capability stack */
  if (!coyotos_SpaceBank_alloc(CR_SPACEBANK,
			       coyotos_Range_obType_otCapPage,
			       coyotos_Range_obType_otInvalid,
			       coyotos_Range_obType_otInvalid,
			       CR_NEWPAGE,
			       CR_NULL,
			       CR_NULL,
			       &_IDL_E) ||
	!coyotos_AddressSpace_setSlot(CR_NEWADDR, 0, CR_NEWPAGE, &_IDL_E))
    goto fail;

  return;

 fail:
  coyotos_SpaceBank_destroyBankAndReturn(CR_SPACEBANK, CR_RETURN,
                                         _IDL_E.errCode,
                                         &_IDL_E);
  return;
}
