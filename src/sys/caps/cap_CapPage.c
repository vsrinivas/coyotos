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
#include <kerninc/string.h>
#include <coyotos/syscall.h>
#include <hal/transmap.h>
#include <hal/vm.h>
#include <hal/syscall.h>
#include <idl/coyotos/CapPage.h>

extern void cap_AddressSpace(InvParam_t* iParam);

void cap_CapPage(InvParam_t *iParam)
{
  size_t slot = ~0u;		/* intentionally illegal value */

  const size_t nSlot = COYOTOS_PAGE_SIZE / sizeof(capability);
  uintptr_t opCode = iParam->opCode;

  switch(opCode) {
  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();

      InvTypeMessage(iParam, IKT_coyotos_CapPage);
      break;
    }

  case OC_coyotos_AddressSpace_erase:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      if (iParam->iCap.cap->restr & (CAP_RESTR_RO|CAP_RESTR_WK)) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_NoAccess);
	return;
      }

      Page *toHdr = (Page *) iParam->iCap.cap->u2.prepObj.target;
      obhdr_dirty(&toHdr->mhdr.hdr);
      
      capability *va = TRANSMAP_MAP(toHdr->pa, capability *);

      sched_commit_point();

      for (size_t i = 0; i < nSlot; i++)
	cap_init(&va[i]);

      TRANSMAP_UNMAP(va);

      iParam->opw[0] = InvResult(iParam, 0);
      break;
    }

  case OC_coyotos_AddressSpace_copyFrom:
    {
      INV_REQUIRE_ARGS(iParam, 1);

      cap_prepare(iParam->srcCap[1].cap);

      if (iParam->srcCap[1].cap->type != ct_CapPage) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }
      if (iParam->iCap.cap->restr & (CAP_RESTR_RO|CAP_RESTR_WK)) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_NoAccess);
	return;
      }

      Page *fromHdr = (Page *) iParam->srcCap[1].cap->u2.prepObj.target;
      Page *toHdr = (Page *) iParam->iCap.cap->u2.prepObj.target;

      obhdr_dirty(&toHdr->mhdr.hdr);
      
      capability *fromVA = TRANSMAP_MAP(fromHdr->pa, capability *);
      capability *toVA = TRANSMAP_MAP(toHdr->pa, capability *);

      sched_commit_point();

      for (size_t i = 0; i < nSlot; i++) {
	/** All readers must lock the capability source page before
	 * reading. Since we hold the lock, our access is exclusive
	 * and it is safe to weaken AFTER copying.
	 */
	cap_set(&toVA[i], &fromVA[i]);
	if (iParam->srcCap[1].cap->restr & CAP_RESTR_WK)
	  cap_weaken(&toVA[i]);
      }

      TRANSMAP_UNMAP(fromVA);
      TRANSMAP_UNMAP(toVA);

      uint32_t from_l2g = iParam->srcCap[1].cap->u1.mem.l2g;
      uint32_t from_match = iParam->srcCap[1].cap->u1.mem.match;

      cap_set(&iParam->srcCap[0].theCap, iParam->iCap.cap);
      iParam->srcCap[0].theCap.u1.mem.l2g = from_l2g;
      iParam->srcCap[0].theCap.u1.mem.match = from_match;

      iParam->opw[0] = InvResult(iParam, 1);

      break;
    }

  case OC_coyotos_AddressSpace_fetch:
  case OC_coyotos_AddressSpace_getSlot:
    {

      size_t last_dw = 2;
#if COYOTOS_HW_ADDRESS_BITS == 32
      last_dw++;
#endif

      if (opCode == OC_coyotos_AddressSpace_fetch) {
	coyaddr_t addr = get_iparam64(iParam);
	INV_REQUIRE_ARGS(iParam, 0);

	if (addr >= COYOTOS_PAGE_SIZE || ((addr % 16) != 0)) {
	  sched_commit_point();

	  InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	  return;
	}
	slot = addr / sizeof(capability);
      }
      else {
	slot = get_iparam32(iParam);
	INV_REQUIRE_ARGS(iParam, 0);
      }

      if (slot >= nSlot) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      Page *pgHdr = (Page *) iParam->iCap.cap->u2.prepObj.target;
      capability *va = TRANSMAP_MAP(pgHdr->pa, capability *);

      sched_commit_point();

      cap_set(&iParam->srcCap[0].theCap, &va[slot]);
      if (iParam->iCap.cap->restr & CAP_RESTR_WK)
	cap_weaken(&iParam->srcCap[0].theCap);

      TRANSMAP_UNMAP(va);

      iParam->opw[0] = InvResult(iParam, 1);
      return;
    }

    /* The following opcodes have no implementation for Pages. */
  case OC_coyotos_AddressSpace_store:
  case OC_coyotos_AddressSpace_guardedSetSlot:
  case OC_coyotos_AddressSpace_setSlot:
    {
      guard_t guard = 0;

      if (opCode == OC_coyotos_AddressSpace_store) {
	coyaddr_t addr = get_iparam64(iParam);
	INV_REQUIRE_ARGS(iParam, 1);

	if (addr >= COYOTOS_PAGE_SIZE || ((addr % 16) != 0)) {
	  sched_commit_point();

	  InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	  return;
	}
	slot = addr / sizeof(capability);
      }
      else {
	slot = get_iparam32(iParam);
	if (opCode == OC_coyotos_AddressSpace_guardedSetSlot) {
	  guard = get_iparam32(iParam);
	  if (!guard_valid(guard)) {
	    sched_commit_point();
	    
	    InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	    return;
	  }     
	}
	INV_REQUIRE_ARGS(iParam, 1);
      }

      if (slot >= nSlot) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }
      if (iParam->iCap.cap->restr & (CAP_RESTR_RO|CAP_RESTR_WK)) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_NoAccess);
	return;
      }

      /* Need to validate target cap type, so must prepare: */
      if (opCode == OC_coyotos_AddressSpace_guardedSetSlot) {
	cap_prepare(iParam->srcCap[1].cap);

	/* Check that it is of a type that accepts a guard: */
	switch (iParam->srcCap[1].cap->type) {
	case ct_Window:
	case ct_LocalWindow:
	case ct_CapPage:
	case ct_Page:
	case ct_GPT:
	  break;
	default:
	  {
	    sched_commit_point();

	    InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	    return;
	  }
	}
      }

      Page *pgHdr = (Page *) iParam->iCap.cap->u2.prepObj.target;
      obhdr_dirty(&pgHdr->mhdr.hdr);

      capability *va = TRANSMAP_MAP(pgHdr->pa, capability *);

      sched_commit_point();

      cap_gc(iParam->srcCap[1].cap);

      cap_set(&va[slot], iParam->srcCap[1].cap);

      if (opCode == OC_coyotos_AddressSpace_guardedSetSlot) {
	va[slot].u1.mem.l2g = guard_l2g(guard);
	va[slot].u1.mem.match = guard_match(guard);
      }

      TRANSMAP_UNMAP(va);

      iParam->opw[0] = InvResult(iParam, 0);

      break;
    }

  case OC_coyotos_AddressSpace_extendedFetch:
    {
      /* Grab the offset to advance next_idw */
      coyaddr_t offset __attribute__((unused)) =
	get_iparam64(iParam);
      uint32_t l2arg = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      /* degenerate implementation -- offset is ignored, because
       * if the invoked capability was a CapPage capability, there
       * wasn't any containing slot.
       */

      if (l2arg < COYOTOS_PAGE_ADDR_BITS) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      sched_commit_point();

      put_oparam32(iParam, 0);	/* l2slot */
      put_oparam32(iParam, 0);	/* perms */
      iParam->opw[0] = InvResult(iParam, 1);
      cap_set(&iParam->srcCap[0].theCap, iParam->iCap.cap);
      return;
    }

  case OC_coyotos_AddressSpace_extendedStore:
    {
      /* Grab offset,guard to advance next_idw */
      coyaddr_t offset  __attribute__((unused)) =
	get_iparam64(iParam);
      uint32_t l2arg = get_iparam32(iParam);
      uint32_t guard   __attribute__((unused)) =
	get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 1);

      sched_commit_point();

      if (l2arg < COYOTOS_PAGE_ADDR_BITS) {
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }
      else {
	InvErrorMessage(iParam, RC_coyotos_AddressSpace_NoSuchSlot);
	return;
      }
    }

  default:
    {
      cap_AddressSpace(iParam);
      break;
    }
  }
}
