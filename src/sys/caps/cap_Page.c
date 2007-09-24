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
#include <idl/coyotos/Page.h>

extern void cap_AddressSpace(InvParam_t* iParam);

void
cap_Page(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  switch(opCode) {

  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();
      InvTypeMessage(iParam, IKT_coyotos_Page);
      return;
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
      if (!obhdr_dirty(&toHdr->mhdr.hdr)) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_NoAccess);
	return;
      }
      
      char *va = TRANSMAP_MAP(toHdr->pa, char *);

      sched_commit_point();

      memset(va, 0, COYOTOS_PAGE_SIZE);
      TRANSMAP_UNMAP(va);

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_AddressSpace_copyFrom:
    {
      INV_REQUIRE_ARGS(iParam, 1);

      cap_prepare(iParam->srcCap[1].cap);

      if (iParam->srcCap[1].cap->type != ct_Page) {
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
      
      char *fromVA = TRANSMAP_MAP(fromHdr->pa, char *);
      char *toVA = TRANSMAP_MAP(toHdr->pa, char *);

      sched_commit_point();

      memcpy(toVA, fromVA, COYOTOS_PAGE_SIZE);
      TRANSMAP_UNMAP(fromVA);
      TRANSMAP_UNMAP(toVA);

      uint32_t from_l2g = iParam->srcCap[1].cap->u1.mem.l2g;
      uint32_t from_match = iParam->srcCap[1].cap->u1.mem.match;

      cap_set(&iParam->srcCap[0].theCap, iParam->iCap.cap);
      iParam->srcCap[0].theCap.u1.mem.l2g = from_l2g;
      iParam->srcCap[0].theCap.u1.mem.match = from_match;

      iParam->opw[0] = InvResult(iParam, 1);
      return;
    }

  case OC_coyotos_AddressSpace_extendedFetch:
    {
      /* Grab the offset to advance next_idw */
      coyaddr_t offset __attribute__((unused)) =
	get_iparam64(iParam);
      uint32_t l2arg = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

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

      InvErrorMessage(iParam, RC_coyotos_AddressSpace_NoSuchSlot);
      return;
    }

  case OC_coyotos_AddressSpace_setSlot:
  case OC_coyotos_AddressSpace_getSlot:
  case OC_coyotos_AddressSpace_guardedSetSlot:
  case OC_coyotos_AddressSpace_store:
  case OC_coyotos_AddressSpace_fetch:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();

      InvErrorMessage(iParam, RC_coyotos_AddressSpace_CapAccessTypeError);
      return;
    }

  default:
    {
      cap_AddressSpace(iParam);
      break;
    }
  }
}
