/*
 * Copyright (C) 2007, The EROS Group, LLC
 *
 * This file is part of the EROS Operating System.
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
#include <kerninc/Endpoint.h>
#include <kerninc/assert.h>
#include <kerninc/StallQueue.h>
#include <hal/syscall.h>
#include <coyotos/syscall.h>
#include <idl/coyotos/Endpoint.h>

extern void cap_Cap(InvParam_t* iParam);

/** Need to wake all of the blocked processes, because they need to
 * redirect to the new recipient or re-process their invocation
 * according to the new prevailing conditions. Note we will not race
 * with their wakeups, because we hold the endpoint object lock.
 */
static void
WakeRecipientSenders(Endpoint *ep)
{
  if (ep->state.recipient.type == ct_Process) {
    Process *curProc = (Process *) ep->state.recipient.u2.prepObj.target;

    SpinHoldInfo shi = spinlock_grab(&curProc->rcvWaitQ.qLock);
    sq_WakeAll(&curProc->rcvWaitQ, false);

    spinlock_release(shi);
  }
}

void cap_Endpoint(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  switch(opCode) {
  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();
      InvTypeMessage(iParam, IKT_coyotos_Endpoint);
      return;
    }

  case OC_coyotos_Endpoint_setRecipient:
    {
      INV_REQUIRE_ARGS(iParam, 1);

      /* Before changing recipient slot, must wake up everyone
       * currently on the rcvWaitQ queue of the current target process,
       * assuming there is one. */
      Endpoint *ep = (Endpoint *)iParam->iCap.cap->u2.prepObj.target;

      cap_prepare(iParam->srcCap[1].cap);
      cap_prepare(&ep->state.recipient);

      obhdr_dirty(&ep->hdr);

      sched_commit_point();

      if(iParam->srcCap[1].cap->type != ct_Process) {
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      // Wake anyone sleeping on the old recipient, since they may
      // have been using us.
      WakeRecipientSenders(ep);

      // If we were null, there may be people waiting on us;  wake them.
      obhdr_wakeAll(&ep->hdr);

      cap_set(&ep->state.recipient, iParam->srcCap[1].cap);

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_Endpoint_setPayloadMatch:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      Endpoint *ep = (Endpoint *)iParam->iCap.cap->u2.prepObj.target;
      if (ep->state.pm == 0)
	obhdr_dirty(&ep->hdr);
      cap_prepare(&ep->state.recipient);

      sched_commit_point();

      /* Contrary to my initial thought, we do NOT actually need to
       * wake recipient senders in this case. They will discover that
       * their capabilities are invalid when they are awakened in the
       * normal order of things.
       *
       * We awaken them here because (presumptively) they will almost
       * all fail the payload test once PM is said, so awakening them
       * allows them to make progress sooner. 
       *
       * However, no point doing this if they have already run the PM
       * gauntlet. 
       */
      if (ep->state.pm == 0) {
	WakeRecipientSenders(ep);

	ep->state.pm = 1;
      }

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_Endpoint_setEndpointID:
    {
      uint64_t epID = get_iparam64(iParam);
      INV_REQUIRE_ARGS(iParam, 0);

      Endpoint *ep = (Endpoint *)iParam->iCap.cap->u2.prepObj.target;

      if (epID != ep->state.endpointID)
	obhdr_dirty(&ep->hdr);

      sched_commit_point();

      /* Changing the endpoint ID does NOT require waking
       * senders. They will re-acquire the endpoint ID when they
       * actually make it through the endpoint during invocation.
       */

      if (epID != ep->state.endpointID)
	ep->state.endpointID = epID;

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_Endpoint_getEndpointID:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      Endpoint *ep = (Endpoint *)iParam->iCap.cap->u2.prepObj.target;

      sched_commit_point();

      put_oparam64(iParam, ep->state.endpointID);
      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_Endpoint_makeEntryCap:
  case OC_coyotos_Endpoint_makeAppNotifier:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      uintptr_t pp = get_pw(iParam->invoker, 2);

      sched_commit_point();

      cap_set(&iParam->srcCap[0].theCap, iParam->iCap.cap);
      iParam->srcCap[0].theCap.type = 
	(opCode == OC_coyotos_Endpoint_makeEntryCap) ? ct_Entry : ct_AppInt;
      iParam->srcCap[0].theCap.u1.protPayload = pp;
      
      iParam->opw[0] = InvResult(iParam, 1);
      return;
    }

  default:
    cap_Cap(iParam);
    break;
  }
}
