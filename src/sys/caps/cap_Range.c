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
#include <kerninc/Endpoint.h>
#include <kerninc/assert.h>
#include <kerninc/ObjectHash.h>
#include <kerninc/ObStore.h>
#include <kerninc/Cache.h>
#include <hal/syscall.h>
#include <coyotos/syscall.h>
#include <idl/coyotos/Range.h>

extern void cap_Cap(InvParam_t* iParam);

void cap_Range(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  switch(opCode) {
  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();
      InvTypeMessage(iParam, IKT_coyotos_Range);
      break;
    }

  case OC_coyotos_Range_nextBackedSubrange:
    {
      uint64_t startOffset __attribute__((unused)) = get_iparam64(iParam);
      uint32_t obType = get_iparam32(iParam);
      ObType oty;

      INV_REQUIRE_ARGS(iParam, 0);

      switch(obType) {
      case coyotos_Range_obType_otPage:
	oty = ot_Page;
	break;
      case coyotos_Range_obType_otCapPage:
	oty = ot_CapPage;
	break;
      case coyotos_Range_obType_otGPT:
	oty = ot_GPT;
	break;
      case coyotos_Range_obType_otProcess:
	oty = ot_Process;
	break;
      case coyotos_Range_obType_otEndpoint:
	oty = ot_Endpoint;
	break;

      default:
	{
	  sched_commit_point();
	  InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	  return;
	}
      }
      
      sched_commit_point();
      put_oparam64(iParam, 0);
      put_oparam64(iParam, Cache.max_oid[oty]);
      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }
  case OC_coyotos_Range_identify:
    {
      INV_REQUIRE_ARGS(iParam, 1);

      cap_prepare(iParam->srcCap[1].cap);
      sched_commit_point();

      uint32_t ot = coyotos_Range_obType_otInvalid;
      oid_t oid = 0;

      switch(iParam->srcCap[1].cap->type) {
      case ct_Page:
	ot = coyotos_Range_obType_otPage;
	break;
      case ct_CapPage:
	ot = coyotos_Range_obType_otCapPage;
	break;
      case ct_GPT:
	ot = coyotos_Range_obType_otGPT;
	break;
      case ct_Process:
	ot = coyotos_Range_obType_otProcess;
	break;
      case ct_Endpoint:
	ot = coyotos_Range_obType_otEndpoint;
	break;
      default:
	break;
      }

      switch(iParam->srcCap[1].cap->type) {
      case ct_Page:
      case ct_CapPage:
      case ct_GPT:
      case ct_Process:
      case ct_Endpoint:
	oid = iParam->srcCap[1].cap->u2.prepObj.target->oid;
      default:
	break;
      }

      put_oparam32(iParam, ot);
      put_oparam64(iParam, ot);
      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_Range_rescind:
    {
      INV_REQUIRE_ARGS(iParam, 1);

      /* If cap is unprepared, we need to bring the object in so
       * that we can bump the allocation count. If cap is prepared,
       * this will do no harm.
       */
      cap_prepare(iParam->srcCap[1].cap);

      ObjectHeader *obHdr = iParam->srcCap[1].cap->u2.prepObj.target;
      bool bumpAllocCount = obHdr->hasDiskCaps ? true : false;

      obhdr_dirty(obHdr);
      obhdr_invalidate(obHdr);

      /** @bug I bet this is completely boogered. */
      if (obHdr->ty == ot_Process) {
	Process *p = (Process *)obHdr;

	/** @bug Need to deal with a corner case here. */
	assert(p != MY_CPU(current));

	spinlock_grab(&p->rcvWaitQ.qLock);
	sq_WakeAll(&p->rcvWaitQ, false);

	Link *rqLink = process_to_link(p);
	if (!link_isSingleton(rqLink))
	  link_unlink(rqLink);
      }
      sched_commit_point();

      obHdr->pinned = 0;
      cache_clear_object(obHdr);

      if (bumpAllocCount) {
	/** @bug Watch out for alloc Count rollover -- object must
	    become broken in that case. */
	obHdr->allocCount++;
      }

      /* Careful! We may have just destroyed either the invoker or the
       * invokee (or both). Unfortunatly, if we destroyed the invoker
       * we still need to deliver results to the invokee (assuming one
       * exists).
       *
       * We may also have destroyed the reply endpoint.
       */

      if (iParam->invokeeEP == (Endpoint *) obHdr)
	iParam->invokee = 0;	/* suppress reply */

      if (iParam->invokee == (Process *) obHdr)
	iParam->invokee = 0;

      if (iParam->invoker == (Process *) obHdr) {
	HoldInfo hi = { &iParam->invoker->hdr.lock, 0 };
	mutex_release(hi);
	iParam->invoker = 0;
	MY_CPU(current) = 0;
      }

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_Range_getCap:
  case OC_coyotos_Range_waitCap:
  case OC_coyotos_Range_getProcess:
  case OC_coyotos_Range_waitProcess:
    {
      oid_t oid = get_iparam64(iParam);
      uint32_t obType = coyotos_Range_obType_otProcess;
      uint32_t nCapArg = 1;

      if ((opCode == OC_coyotos_Range_getCap) ||
	  (opCode == OC_coyotos_Range_waitCap)) {
	nCapArg = 0;
	obType = get_iparam32(iParam);
      }

      INV_REQUIRE_ARGS(iParam, nCapArg);

      bool waitForRange = 
	((opCode == OC_coyotos_Range_waitCap) || 
	 (opCode == OC_coyotos_Range_waitProcess));

      uint32_t capType = 0;
      capability *brand = NULL;

      /* handle physical ranges as a special case */
      if (oid >= coyotos_Range_physOidStart) {
	kpa_t kpa = (oid - coyotos_Range_physOidStart) * COYOTOS_PAGE_SIZE;
	/* check for bad type or overflow */
	if (obType != coyotos_Range_obType_otPage ||
	    (kpa / COYOTOS_PAGE_SIZE + coyotos_Range_physOidStart) != oid) {
	  sched_commit_point();
	  InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	  return;
	}
	Page *retVal = cache_get_physPage(kpa);
	if (retVal == 0) {
	  sched_commit_point();
	  InvErrorMessage(iParam, RC_coyotos_Range_RangeErr);
	  return;
	}

	INIT_TO_ZERO(&iParam->srcCap[0].theCap);

	/* Set up a deprepared cap, then prepare it. */
	iParam->srcCap[0].theCap.type = ct_Page;
	iParam->srcCap[0].theCap.swizzled = 0;
	iParam->srcCap[0].theCap.restr = 0;
	
	iParam->srcCap[0].theCap.allocCount = retVal->mhdr.hdr.allocCount;
	
	/* Memory caps have a min GPT requirement. */
	iParam->srcCap[0].theCap.u1.mem.l2g = COYOTOS_PAGE_ADDR_BITS;

	iParam->srcCap[0].theCap.u2.oid = retVal->mhdr.hdr.oid;

	cap_prepare(&iParam->srcCap[0].theCap);
	
	sched_commit_point();

	retVal->mhdr.hdr.pinned = 1;

	iParam->opw[0] = InvResult(iParam, 1);
	return;
      }

      /** We need to hunt down the target object so that we can
       * determine the allocation count.  If the object is not already
       * in core we need to bring it in.
       *
       * It is tempting to think that we could avoid this complication
       * in the embedded implementation by pre-assigning object IDs to
       * the elements of the various object vectors. Regrettably this
       * is not so. If we do that we will lose the ability to allocate
       * objects at known physical addresses.
       *
       * Given that we cannot simplify this much for the embedded
       * case, we decided to go ahead and keep the code as close to
       * the general implementation as possible to lower code
       * maintainence complexity. I @em did take the expedient of
       * allocating the objects here directly rather than invoking the
       * ObStore logic to do it.
       */

      switch(obType) {
      case coyotos_Range_obType_otPage:
	capType = ct_Page;
	break;
      case coyotos_Range_obType_otCapPage:
	capType = ct_CapPage;
	break;
      case coyotos_Range_obType_otGPT:
	capType = ct_GPT;
	break;
      case coyotos_Range_obType_otProcess:
	capType = ct_Process;
	break;
      case coyotos_Range_obType_otEndpoint:
	capType = ct_Endpoint;
	break;

      default:
	{
	  sched_commit_point();
	  InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	  return;
	}
      }

      ObType ot = captype_to_obtype(capType);

      ObjectHeader *obHdr = 
	obstore_require_object(ot, oid, waitForRange, NULL);

      if (obHdr == 0) {
	  sched_commit_point();
	  InvErrorMessage(iParam, RC_coyotos_Range_RangeErr);
	  return;
      }
      INIT_TO_ZERO(&iParam->srcCap[0].theCap);

      /* Set up a deprepared cap, then prepare it. */
      iParam->srcCap[0].theCap.type = capType;
      iParam->srcCap[0].theCap.swizzled = 0;
      iParam->srcCap[0].theCap.restr = 0;

      iParam->srcCap[0].theCap.allocCount = obHdr->allocCount;

      /* Memory caps have a min GPT requirement. */
      if (capType == ct_Page || capType == ct_CapPage || capType == ct_GPT)
	iParam->srcCap[0].theCap.u1.mem.l2g = COYOTOS_PAGE_ADDR_BITS;

      iParam->srcCap[0].theCap.u2.oid = obHdr->oid;

      cap_prepare(&iParam->srcCap[0].theCap);

      if (brand)
	obhdr_dirty(obHdr);

      sched_commit_point();

      if (brand) {
	assert(obType == coyotos_Range_obType_otProcess);
	Process *p = (Process *) obHdr;
	cap_set(&p->state.brand, brand);
      }

      iParam->opw[0] = InvResult(iParam, 1);
      return;
    }

  default:
    cap_Cap(iParam);
    break;
  }
}
