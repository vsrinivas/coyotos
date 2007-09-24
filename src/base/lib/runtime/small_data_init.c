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
#define CR_SMASHED(n)	CR_APP(CRN_LASTAPP_STABLE - CRN_FIRSTAPP + 1 + (n))

#define CR_NEWADDR	CR_SMASHED(0) /* new address space */
#define CR_NEWPAGE	CR_SMASHED(1) /* available for new page */
#define CR_OLDADDR	CR_SMASHED(2) /* old address space */
#define CR_OLDPAGE	CR_SMASHED(3) /* old page */

/* HACK to make sure sbrk is included if the runtime hook is included. */
extern char *sbrk(size_t);
char *(*sbrk_ptr)(size_t) = sbrk;

void
__small_data_init(uintptr_t data, uintptr_t end)
{
  size_t cur;
  size_t first_slot = data / COYOTOS_PAGE_SIZE;
  size_t last_slot = (end + COYOTOS_PAGE_SIZE - 1) / COYOTOS_PAGE_SIZE;

  /* Unlike most C routines, this code is run before the data segment is
   * writable (in fact, its job in life is to make the data segment
   * writable).  So it can't use the default IDL_Environment that the
   * normal wrappers use.  Instead, it must define an environment on the
   * stack, and use the IDL_ENV_* varients of the wrappers, which allow
   * you to specify an environment to use.
   *
   * Note that we hard-code the replyCap and epID to the runtime defaults;
   * since we're running before the program gets control, this is
   * safe.
   */
  IDL_Environment local_env = {
    .replyCap = CR_REPLYEPT,
    .epID = 0
  };

  for (cur = first_slot; cur < last_slot; cur++) {
    if (!IDL_ENV_coyotos_AddressSpace_getSlot(CR_OLDADDR, cur, CR_OLDPAGE,
					      &local_env) ||
	!IDL_ENV_coyotos_SpaceBank_alloc(CR_SPACEBANK,
					 coyotos_Range_obType_otPage,
					 coyotos_Range_obType_otInvalid,
					 coyotos_Range_obType_otInvalid,
					 CR_NEWPAGE,
					 CR_NULL,
					 CR_NULL, &local_env) ||
	!IDL_ENV_coyotos_AddressSpace_copyFrom(CR_NEWPAGE, 
					       CR_OLDPAGE, 
					       CR_NEWPAGE, &local_env) ||
	!IDL_ENV_coyotos_AddressSpace_setSlot(CR_NEWADDR, 
					      cur,
					      CR_NEWPAGE, &local_env))
      goto fail;
  }

  /* install a CapPage in slot 0 for the capability stack */
  if (!IDL_ENV_coyotos_SpaceBank_alloc(CR_SPACEBANK,
				       coyotos_Range_obType_otCapPage,
				       coyotos_Range_obType_otInvalid,
				       coyotos_Range_obType_otInvalid,
				       CR_NEWPAGE,
				       CR_NULL,
				       CR_NULL, &local_env) ||
      !IDL_ENV_coyotos_AddressSpace_setSlot(CR_NEWADDR, 
					    0, 
					    CR_NEWPAGE, &local_env))
    goto fail;

  return;

 fail:
  IDL_ENV_coyotos_SpaceBank_destroyBankAndReturn(CR_SPACEBANK, CR_RETURN,
						 local_env.errCode,
						 &local_env);
  return;
}
