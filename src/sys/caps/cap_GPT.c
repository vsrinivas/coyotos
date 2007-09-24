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
#include <kerninc/GPT.h>
#include <kerninc/util.h>
#include <kerninc/string.h>
#include <kerninc/MemWalk.h>
#include <kerninc/Depend.h>
#include <coyotos/syscall.h>
#include <hal/transmap.h>
#include <hal/vm.h>
#include <hal/syscall.h>
#include <idl/coyotos/GPT.h>

extern void cap_AddressSpace(InvParam_t* iParam);

static uintptr_t fc_to_rc(coyotos_Process_FC fc)
{
  bug("Not handling walk faults yet\n");
}

void cap_GPT(InvParam_t *iParam)
{
  size_t slot = ~0u;		/* intentionally illegal value */

  uintptr_t opCode = iParam->opCode;

  switch(opCode) {
  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();
      InvTypeMessage(iParam, IKT_coyotos_GPT);
      return;
    }

  case OC_coyotos_GPT_setl2v:
    {
      uint32_t l2v = get_iparam32(iParam);
      INV_REQUIRE_ARGS(iParam, 0);

      GPT *gpt = (GPT *)iParam->iCap.cap->u2.prepObj.target;

      require(obhdr_dirty(&gpt->mhdr.hdr));

      /* We must invalidate all cached state. */
      memhdr_invalidate_cached_state(&gpt->mhdr);

      sched_commit_point();

      gpt->state.l2v = l2v;
      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_GPT_getl2v:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      GPT *gpt = (GPT *)iParam->iCap.cap->u2.prepObj.target;

      sched_commit_point();

      put_oparam32(iParam, gpt->state.l2v);
      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_GPT_setHandler:
    {
      /** @bug is this really an iparam32? */
      uint32_t hasHandler = get_iparam32(iParam);
      INV_REQUIRE_ARGS(iParam, 0);

      GPT *gpt = (GPT *)iParam->iCap.cap->u2.prepObj.target;

      obhdr_dirty(&gpt->mhdr.hdr);

      if (gpt->state.ha && hasHandler == 0)
	cap_handlerBeingOverwritten(&gpt->state.cap[GPT_HANDLER_SLOT]);

      sched_commit_point();

      gpt->state.ha = !!hasHandler;
      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_GPT_getHandler:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      GPT *gpt = (GPT *)iParam->iCap.cap->u2.prepObj.target;

      sched_commit_point();

      put_oparam32(iParam, gpt->state.ha);
      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_GPT_makeLocalWindow:
  case OC_coyotos_GPT_makeBackgroundWindow:
    {
      uint32_t localRoot = 0; 

      uint32_t slot = get_iparam32(iParam);
      uint32_t restr = get_iparam32(iParam);
      uint64_t offset = get_iparam64(iParam);
      uint32_t guard = get_iparam32(iParam);
      if (opCode == OC_coyotos_GPT_makeLocalWindow)
	localRoot = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      GPT *gpt = (GPT *)iParam->iCap.cap->u2.prepObj.target;

      if ((restr & ~CAP_RESTR_MASK) || 
	  (slot >= NUM_GPT_SLOTS) ||
	  (localRoot >= NUM_GPT_SLOTS) ||
	  !guard_valid(guard) ||
	  safe_low_bits(offset, guard_l2g(guard)) != 0) {
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      obhdr_dirty(&gpt->mhdr.hdr);

      sched_commit_point();

      iParam->opw[0] = InvResult(iParam, 0);

      depend_invalidate_slot(gpt, slot);

      cap_init(&gpt->state.cap[slot]);
      if (opCode == OC_coyotos_GPT_makeLocalWindow) {
	gpt->state.cap[slot].type = ct_LocalWindow;
	gpt->state.cap[slot].allocCount = localRoot;
      }
      else
	gpt->state.cap[slot].type = ct_Window;

      gpt->state.cap[slot].restr = restr;
      gpt->state.cap[slot].u1.mem.l2g = guard_l2g(guard);
      gpt->state.cap[slot].u1.mem.match = guard_match(guard);
      gpt->state.cap[slot].u2.offset = offset;

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

      GPT *toGPT = (GPT *) iParam->iCap.cap->u2.prepObj.target;
      obhdr_dirty(&toGPT->mhdr.hdr);
      
      sched_commit_point();

      /* We must invalidate all cached state. */
      memhdr_invalidate_cached_state(&toGPT->mhdr);

      for (size_t i = 0; i < NUM_GPT_SLOTS; i++)
	cap_init(&toGPT->state.cap[i]);

      toGPT->state.l2v = COYOTOS_PAGE_ADDR_BITS;
      toGPT->state.ha = 0;
      toGPT->state.bg = 0;

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_AddressSpace_copyFrom:
    {
      INV_REQUIRE_ARGS(iParam, 1);

      cap_prepare(iParam->srcCap[1].cap);

      if (iParam->srcCap[1].cap->type != ct_GPT ||
	  (iParam->srcCap[1].cap->restr & CAP_RESTR_OP)) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }
      if (iParam->iCap.cap->restr & (CAP_RESTR_RO|CAP_RESTR_WK)) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_NoAccess);
	return;
      }

      GPT *fromGPT = (GPT *) iParam->srcCap[1].cap->u2.prepObj.target;
      GPT *toGPT = (GPT *) iParam->iCap.cap->u2.prepObj.target;

      obhdr_dirty(&toGPT->mhdr.hdr);
      
      sched_commit_point();

      /* We must invalidate all cached state. */
      memhdr_invalidate_cached_state(&toGPT->mhdr);

      for (size_t i = 0; i < NUM_GPT_SLOTS; i++) {
	cap_set(&toGPT->state.cap[i], &fromGPT->state.cap[i]);
	if (iParam->srcCap[1].cap->restr & CAP_RESTR_WK)
	  cap_weaken(&toGPT->state.cap[i]);
      }

      toGPT->state.l2v = fromGPT->state.l2v;
      toGPT->state.ha = fromGPT->state.ha;
      toGPT->state.bg = fromGPT->state.bg;

      uint32_t from_l2g = iParam->srcCap[1].cap->u1.mem.l2g;
      uint32_t from_match = iParam->srcCap[1].cap->u1.mem.match;

      cap_set(&iParam->srcCap[0].theCap, iParam->iCap.cap);
      iParam->srcCap[0].theCap.u1.mem.l2g = from_l2g;
      iParam->srcCap[0].theCap.u1.mem.match = from_match;

      iParam->opw[0] = InvResult(iParam, 1);
      return;
    }

  case OC_coyotos_AddressSpace_getSlot:
    {
      slot = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      if (slot >= NUM_GPT_SLOTS) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      GPT *fromGPT = (GPT *) iParam->iCap.cap->u2.prepObj.target;
      
      sched_commit_point();

      cap_set(&iParam->srcCap[0].theCap, &fromGPT->state.cap[slot]);
      if (iParam->iCap.cap->restr & (CAP_RESTR_RO|CAP_RESTR_WK))
	cap_weaken(&iParam->srcCap[0].theCap);

      iParam->opw[0] = InvResult(iParam, 1);
      return;
    }

  case OC_coyotos_AddressSpace_setSlot:
  case OC_coyotos_AddressSpace_guardedSetSlot:
    {
      slot = get_iparam32(iParam);
      guard_t guard = 0;
      if (opCode == OC_coyotos_AddressSpace_guardedSetSlot) {
	guard = get_iparam32(iParam);
	if (!guard_valid(guard)) {
	  sched_commit_point();
	
	  InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	  return;
	}     
      }

      INV_REQUIRE_ARGS(iParam, 1);

      if (slot >= NUM_GPT_SLOTS) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      if (iParam->iCap.cap->restr & (CAP_RESTR_RO|CAP_RESTR_WK)) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_NoAccess);
	return;
      }

      GPT *toGPT = (GPT *) iParam->iCap.cap->u2.prepObj.target;

      /* Someone may be sleeping on this handler; to help with forward
       * progress, wake them up.  This has no effect on our security
       * guarantees. 
       */
      if (toGPT->state.ha && slot == GPT_HANDLER_SLOT)
	cap_handlerBeingOverwritten(&toGPT->state.cap[slot]);
      
      sched_commit_point();

      depend_invalidate_slot(toGPT, slot);
      cap_set(&toGPT->state.cap[slot], iParam->srcCap[1].cap);

      if (opCode == OC_coyotos_AddressSpace_guardedSetSlot) {
	toGPT->state.cap[slot].u1.mem.l2g = guard_l2g(guard);
	toGPT->state.cap[slot].u1.mem.match = guard_match(guard);
      }

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_AddressSpace_fetch:
    {
      coyaddr_t addr = get_iparam64(iParam);
      INV_REQUIRE_ARGS(iParam, 0);

      if ((addr % 16) != 0) {
	sched_commit_point();

	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      slot = addr & ~COYOTOS_PAGE_ADDR_MASK;

      MemWalkResults mwr;
      coyotos_Process_FC fc =
	memwalk(iParam->iCap.cap,
		addr,
		false,		/* not for writing */
		&mwr);

      if (fc)
	/* ??? */
	bug("Not handling walk faults yet\n");

      MemWalkEntry *last = &mwr.ents[mwr.count - 1];

      if (mwr.cum_restr & CAP_RESTR_OP) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_AddressSpace_OpaqueSpace);
	return;
      }
      else if (last->entry->hdr.ty != ot_CapPage) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_AddressSpace_CapAccessTypeError);
	return;
      }

      sched_commit_point();

      /* Ready to read from the cap page. */
      Page *pgHdr = (Page *) last->entry;
      capability *capPg = TRANSMAP_MAP(pgHdr->pa, capability *);

      cap_set(&iParam->srcCap[0].theCap, &capPg[slot]);
      if (mwr.cum_restr & CAP_RESTR_WK)
	cap_weaken(&iParam->srcCap[0].theCap);

      TRANSMAP_UNMAP(capPg);

      iParam->opw[0] = InvResult(iParam, 1);
      return;
    }

    /* The following opcodes have no implementation for Pages. */
  case OC_coyotos_AddressSpace_store:
    {
      coyaddr_t addr = get_iparam64(iParam);

      INV_REQUIRE_ARGS(iParam, 1);

      if ((addr % 16) != 0) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      slot = addr & ~COYOTOS_PAGE_ADDR_MASK;

      MemWalkResults mwr;
      coyotos_Process_FC fc =
	memwalk(iParam->iCap.cap,
		addr,
		true,		/* for writing */
		&mwr);

      if (fc)
	/* ??? */
	bug("Not handling walk faults yet\n");

      MemWalkEntry *last = &mwr.ents[mwr.count - 1];

      if (mwr.cum_restr & (CAP_RESTR_RO|CAP_RESTR_WK)) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_Cap_NoAccess);
	return;
      }
      else if (last->entry->hdr.ty != ot_CapPage) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_AddressSpace_CapAccessTypeError);
	return;
      }

      sched_commit_point();

      /* Ready to write to the cap page. */
      Page *pgHdr = (Page *) last->entry;
      capability *capPg = TRANSMAP_MAP(pgHdr->pa, capability *);

      cap_set(&capPg[slot], iParam->srcCap[1].cap);

      TRANSMAP_UNMAP(capPg);
      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_AddressSpace_extendedFetch:
    {
      coyaddr_t offset = get_iparam64(iParam);
      uint32_t l2arg = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      if ((l2arg > COYOTOS_SOFTADDR_BITS) ||
	  (l2arg < COYOTOS_PAGE_ADDR_BITS) ||
	  ((l2arg == COYOTOS_SOFTADDR_BITS) && offset) ||
	  (offset & ((1ull << l2arg) - 1))) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      /*      slot = addr & ~COYOTOS_PAGE_ADDR_MASK;  */

      MemWalkResults mwr;
      coyotos_Process_FC fc =
	extended_memwalk(iParam->iCap.cap,
			 offset,
			 l2arg,
			 false,		/* not for writing */
			 &mwr);

      if (mwr.cum_restr & CAP_RESTR_OP) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_AddressSpace_OpaqueSpace);
	return;
      }

      if (mwr.count == 0) {
	sched_commit_point();
	
	// return the invoked capability.
	cap_set(&iParam->srcCap[0].theCap, iParam->iCap.cap);
	put_oparam32(iParam, 0); /* slot */
	put_oparam32(iParam, 0); /* perms */
	iParam->opw[0] = InvResult(iParam, 1);
	return;
      }
	
      if (fc) {
	sched_commit_point();
	InvErrorMessage(iParam, fc_to_rc(fc));
	return;
      }

      sched_commit_point();

      MemWalkEntry *last = &mwr.ents[mwr.count - 1];
      assert (last->entry->hdr.ty == ot_GPT);

      GPT *gpt = (GPT *) last->entry;

      cap_set(&iParam->srcCap[0].theCap, &gpt->state.cap[last->slot]);
      if (mwr.cum_restr & CAP_RESTR_WK)
	cap_weaken(&iParam->srcCap[0].theCap);

      put_oparam32(iParam, gpt->state.l2v); /* slot */
      put_oparam32(iParam, mwr.cum_restr); /* perms */
      iParam->opw[0] = InvResult(iParam, 1);

      return;
    }
  case OC_coyotos_AddressSpace_extendedStore:
    {
      coyaddr_t offset = get_iparam64(iParam);
      uint32_t l2arg = get_iparam32(iParam);
      guard_t guard =  get_iparam32(iParam);

      uint32_t l2g = guard_l2g(guard);
      uint32_t match = guard_match(guard);

      INV_REQUIRE_ARGS(iParam, 1);

      if ((l2arg > COYOTOS_SOFTADDR_BITS) ||
	  (l2arg < COYOTOS_PAGE_ADDR_BITS) ||
	  ((l2arg == COYOTOS_SOFTADDR_BITS) && offset) ||
	  (offset & ((1ull << l2arg) - 1)) ||
	  !guard_valid(guard)) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      /*      slot = addr & ~COYOTOS_PAGE_ADDR_MASK;  */

      MemWalkResults mwr;
      coyotos_Process_FC fc =
	extended_memwalk(iParam->iCap.cap,
			 offset,
			 l2arg,
			 true,		/* for writing */
			 &mwr);

      if (mwr.cum_restr & CAP_RESTR_OP) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_AddressSpace_OpaqueSpace);
	return;
      }

      if (fc) {
	sched_commit_point();
	InvErrorMessage(iParam, fc_to_rc(fc));
	return;
      }

      // If no appropriate GPT was found, bail out.
      if (mwr.count == 0 || 
	  mwr.ents[mwr.count - 1].l2v != l2arg) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_AddressSpace_NoSuchSlot);
	return;
      }

      MemWalkEntry *last = &mwr.ents[mwr.count - 1];
      assert (last->entry->hdr.ty == ot_GPT);

      GPT *gpt = (GPT *) last->entry;

      obhdr_dirty(&gpt->mhdr.hdr);

      /** @bug Check last->slot for validity. */
      sched_commit_point();

      depend_invalidate_slot(gpt, last->slot);
      cap_set(&gpt->state.cap[last->slot], iParam->srcCap[1].cap);
      if (l2g != 0) {
	gpt->state.cap[last->slot].u1.mem.l2g = l2g;
	gpt->state.cap[last->slot].u1.mem.match = match;
      }

      iParam->opw[0] = InvResult(iParam, 0);

      return;
    }

  default:
    {
      cap_AddressSpace(iParam);
      break;
    }
  }
}
