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
#include <kerninc/ReadyQueue.h>
#include <kerninc/Endpoint.h>
#include <kerninc/RevMap.h>
#include <kerninc/string.h>
#include <coyotos/syscall.h>
#include <hal/syscall.h>
#include <idl/coyotos/Process.h>

extern void cap_Cap(InvParam_t* iParam);

void cap_ProcessCommon(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  Process *p = (Process *) iParam->iCap.cap->u2.prepObj.target;

  /** @bug This returns the wrong error code if the user sent bad
   * parameters. */
  if ((opCode != OC_coyotos_Process_resume) && 
      (iParam->iCap.cap->restr & CAP_RESTR_RESTART)) {
    sched_commit_point();

    InvErrorMessage(iParam, RC_coyotos_Cap_NoAccess);
    return;
  }


  switch(opCode) {

  case OC_coyotos_Process_resume:
    {
      uint32_t cancelFault = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      proc_ensure_exclusive(p);

      /* A restart capbility can only be used to cancel MEMORY
       * faults. Here is why:
       *
       * Unlike KeyKOS/EROS resume capabilities, the Coyotos restart
       * capability is durable when invoked. This means that if a
       * hostile memory fault handler is ever called, it can later
       * clear faults that it did not receive. If this occurs, there
       * is no good way to resolve it short of cloning the process
       * object and destroying the old one.
       *
       * We don't worry about this excessively for two reasons:
       *
       *  1. If you called that memory keeper in the first place, you
       *     have already decided that you rely on its good behavior.
       *
       *  2. Memory faults are restartable.
       *
       * The second point serves as bug mitigation. If a memory fault
       * handler restarts a faulted process erroneously, that process
       * will re-execute its memory fault without serious harm.
       */
      if (cancelFault && (iParam->iCap.cap->restr & CAP_RESTR_RESTART)) {
	switch(p->state.faultCode) {
	case coyotos_Process_FC_InvalidAddr:
	case coyotos_Process_FC_NoExecute:
	case coyotos_Process_FC_AccessViolation:
	case coyotos_Process_FC_DataAccessTypeError:
	case coyotos_Process_FC_CapAccessTypeError:
	  /* This was a memory fault. A restart capability is
	     permitted to restart this process. */
	  break;
	default:
	  {
	    sched_commit_point();
	    InvErrorMessage(iParam, RC_coyotos_Cap_NoAccess);
	    return;
	  }
	}
      }

      obhdr_dirty(&p->hdr);

      sched_commit_point();

      if (cancelFault) {
	p->state.faultCode = 0;
	p->state.faultInfo = 0;
      }

      if (p->state.runState != PRS_RUNNING) {
	p->state.runState = PRS_RUNNING;

	/** @bug: What to do if already enqueued somewhere? */

	/* Stick target on ready queue: */
	rq_add(&mainRQ, p, false);

	/* If target is invokee, invokee now in wrong state for
	 * response. Suppress reply in this case. */
	if (iParam->invokee == p)
	  iParam->invokee = 0;
      }

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_Process_getSlot:
    {
      uint32_t slot = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      /* Capability slots cannot change while we hold the Process' lock.
       */

      capability *pSlot = 0;
      switch (slot) {
      case coyotos_Process_cslot_handler:
	pSlot = &p->state.handler;
	break;
      case coyotos_Process_cslot_addrSpace:
	pSlot = &p->state.addrSpace;
	break;
      case coyotos_Process_cslot_schedule:
	pSlot = &p->state.schedule;
	break;
      case coyotos_Process_cslot_ioSpace:
	pSlot = &p->state.ioSpace;
	break;
      case coyotos_Process_cslot_cohort:
	pSlot = &p->state.cohort;
	break;
      default:
	break;
      }

      sched_commit_point();

      if (pSlot == 0) {
	InvErrorMessage(iParam,  RC_coyotos_Cap_RequestError);
	return;
      }

      cap_set(&iParam->srcCap[0].theCap, pSlot);
      iParam->opw[0] = InvResult(iParam, 1);
      return;
    }

  case OC_coyotos_Process_setSlot:
    {
      uint32_t slot = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 1);

      proc_ensure_exclusive(p);

      capability *pSlot = 0;
      switch (slot) {
      case coyotos_Process_cslot_handler:
	pSlot = &p->state.handler;
	cap_handlerBeingOverwritten(pSlot);
	break;
      case coyotos_Process_cslot_addrSpace:
	rm_whack_process(p);
	pSlot = &p->state.addrSpace;
	break;
      case coyotos_Process_cslot_schedule:
	p->issues |= pi_Schedule;
	pSlot = &p->state.schedule;
	break;
      case coyotos_Process_cslot_ioSpace:
	pSlot = &p->state.ioSpace;
	break;
      case coyotos_Process_cslot_cohort:
	pSlot = &p->state.cohort;
	break;
      default:
	break;
      }

      obhdr_dirty(&p->hdr);

      sched_commit_point();

      /* This is a bit tricky, because we might be changing the
       * address space slot of the recipient. That is okay, because
       * none of the out-bound parameters in this case are memory
       * parameters! */
      if (pSlot == 0) {
	InvErrorMessage(iParam,  RC_coyotos_Cap_RequestError);
	return;
      }

      iParam->opw[0] = InvResult(iParam, 0);
      cap_set(pSlot, iParam->srcCap[1].cap);
      return;
    }

  case OC_coyotos_Process_getCapReg: 
    {
      uint32_t slot = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();

      if (slot >= NUM_CAP_REGS) {
	InvErrorMessage(iParam,  RC_coyotos_Cap_RequestError);
	return;
      }

      iParam->opw[0] = InvResult(iParam, 1);

      if (slot == 0)
	cap_init(&iParam->srcCap[0].theCap);
      else
	cap_set(&iParam->srcCap[0].theCap, &p->state.capReg[slot]);

      return;
    }
    
  case OC_coyotos_Process_setCapReg:
    {
      uint32_t slot = get_iparam32(iParam);

      /* no need for exclusive access, since holding the Process lock prevents
       * the thread from accessing its capability registers.
       */
      INV_REQUIRE_ARGS(iParam, 1);

      sched_commit_point();

      if (slot == 0 || slot >= NUM_CAP_REGS) {
	InvErrorMessage(iParam,  RC_coyotos_Cap_RequestError);
	return;
      }

      iParam->opw[0] = InvResult(iParam, 0);
      cap_set(&p->state.capReg[slot], iParam->srcCap[1].cap);
      return;
    }

  case OC_coyotos_Process_getState:
    {
      INV_REQUIRE_ARGS(iParam, 0);
      
      sched_commit_point();

      put_oparam8(iParam, p->state.faultCode);
      put_oparam64(iParam, p->state.faultInfo);

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }
  case OC_coyotos_Process_setState:
    {
      uint8_t faultCode = get_iparam8(iParam);
      coyaddr_t faultInfo = get_iparam64(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();

      /** @bug should this do bounds-checking on faultCode? */
      if (faultCode == coyotos_Process_FC_NoFault && faultInfo != 0) {
	InvErrorMessage(iParam,  RC_coyotos_Cap_RequestError);
	return;
      }

      p->state.faultCode = faultCode;
      p->state.faultInfo = faultInfo;

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }
  case OC_coyotos_Process_identifyEntry:
    {
      INV_REQUIRE_ARGS(iParam, 1);

      bool result = false;
      uint32_t pp = 0u;
      uint64_t epID = 0ull;
      bool isMe = false;

      cap_prepare(iParam->srcCap[1].cap);

      if (iParam->srcCap[1].cap->type == ct_Entry) {
	Endpoint *ep = (Endpoint *)iParam->srcCap[1].cap->u2.prepObj.target;

	if ((ep->state.pm == 0) || (ep->state.protPayload == pp)) {
	  capability *pCap = &ep->state.recipient;
	  
	  cap_prepare(pCap);

	  if (pCap->type == ct_Process) {
	    Process *pEntryTarget = 
	      (Process *) pCap->u2.prepObj.target;

	    if (pEntryTarget == p) {
	      result = true;
	      isMe = true;
	    }
	    else {
	      /* Prepare both brand capabilities so that we can compare
	       * them. */
	      cap_prepare(&p->state.brand);
	      cap_prepare(&pEntryTarget->state.brand);
	      
	      if (memcmp(&p->state.brand, &pEntryTarget->state.brand, 
			 sizeof(capability)) == 0)
		result = true;
	    }
	  }
	}
	if (result == true) {
	  pp = iParam->srcCap[1].cap->u1.protPayload;
	  epID = ep->state.endpointID;
	}
      }
      
      sched_commit_point();

      put_oparam32(iParam, pp);
      put_oparam64(iParam, epID);
      put_oparam32(iParam, isMe);
      put_oparam32(iParam, result);

      iParam->opw[0] = InvResult(iParam, 0);
      break;
    }
  case OC_coyotos_Process_amplifyCohortEntry:
    {
      /* Note this is a tricky case, because there are both input and
	 output capabilities to consider. iParam->srcCap[0].theCap is
	 overwritten, and this must not happen until we are done with
	 the input argument. */
      INV_REQUIRE_ARGS(iParam, 1);

      bool result = false;
      uint32_t pp = 0u;
      uint64_t epID = 0ull;

      cap_prepare(iParam->srcCap[1].cap);

      if (iParam->srcCap[1].cap->type == ct_Entry) {
	Endpoint *ep = (Endpoint *)iParam->srcCap[1].cap->u2.prepObj.target;

	if ((ep->state.pm == 0) || (ep->state.protPayload == pp)) {
	  capability *pCap = &ep->state.recipient;

	  cap_prepare(pCap);
	  if (pCap->type == ct_Process) {
	    Process *pEntryTarget = (Process *) pCap->u2.prepObj.target;

	    if (pEntryTarget == p)
	      result = true;
	    else {
	      /* Prepare both cohort capabilities so that we can compare
	       * them. */
	      cap_prepare(&p->state.cohort);
	      cap_prepare(&pEntryTarget->state.cohort);

	      if (memcmp(&p->state.cohort, &pEntryTarget->state.cohort,
			 sizeof(capability)) == 0)
		result = true;
	    }
	  }
	  if (result == true) {
	    pp = iParam->srcCap[1].cap->u1.protPayload;
	    epID = ep->state.endpointID;
	    cap_set(&iParam->srcCap[0].theCap, pCap);
	  }
	  else
	    cap_init(&iParam->srcCap[0].theCap);
	}
      }
      
      sched_commit_point();

      put_oparam32(iParam, pp);
      put_oparam64(iParam, epID);
      put_oparam32(iParam, result);

      iParam->opw[0] = InvResult(iParam, 0);
      break;
    }
  case OC_coyotos_Process_setSpaceAndPC:
    {
      uintptr_t newPC = get_iparam64(iParam);

      INV_REQUIRE_ARGS(iParam, 1);

      Process *p = (Process *) iParam->iCap.cap->u2.prepObj.target;

      proc_ensure_exclusive(p);

      bug("need to invalidate address space");
      obhdr_dirty(&p->hdr);

      sched_commit_point();

      if (p == iParam->invoker) {
	uintptr_t icw = get_icw(iParam->invoker);

	/* Hand-mark send and receive phases complete: */
	icw &= ~(IPW0_RP | IPW0_SP);
	icw = IPW0_WITH_LDW(icw, 0);

	set_icw(iParam->invoker, icw);
      }

      FIX_PC(p->state.fixregs) = newPC;
      cap_set(&p->state.addrSpace, iParam->srcCap[1].cap);

      if (p != iParam->invoker) {
	/// @bug: Need to do this correctly.
	// sq_unlink(p->hdr.queue_link);

	p->state.runState = PRS_RUNNING;
	/// @bug: What locks do we require here?
	rq_add(&mainRQ, p, false);
      }

      if (p == iParam->invokee)
	// Invokee no longer receiveing.
	iParam->invokee = 0;

      if (p == iParam->invoker)
	// Invoker is logically waiting. Resume them in running
	// state.
	sched_restart_transaction();

      // In the unusual case that this was third-party, set up return values:
      iParam->opw[0] = InvResult(iParam, 0);

      break;
    }
  default:
    {
      cap_Cap(iParam);
      break;
    }
  }
}
