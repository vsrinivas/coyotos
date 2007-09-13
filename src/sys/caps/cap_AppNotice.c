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
#include <kerninc/Endpoint.h>
#include <kerninc/ReadyQueue.h>
#include <coyotos/syscall.h>
#include <hal/syscall.h>
#include <idl/coyotos/AppNotice.h>

extern void cap_Cap(InvParam_t* iParam);

void cap_AppNotice(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  switch(opCode) {
  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();

      InvTypeMessage(iParam, IKT_coyotos_AppNotice);
      break;
    }

  case OC_coyotos_AppNotice_postNotice:
    {
      uint32_t notices = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      iParam->invokee = 0;	/* method is oneway */

      notices &= iParam->iCap.cap->u1.protPayload;

      Endpoint *ep = (Endpoint *) iParam->iCap.cap->u2.prepObj.target;
      capability *pCap = &ep->state.recipient;
      cap_prepare(pCap);

      /* Enforced by endpoint setTarget() method and by MKIMAGE. */
      assert ((pCap->type == ct_Null) || (pCap->type == ct_Process));
  
      /* Endpoint may contain Null recipient cap if target process was
	 destroyed. Drop this in that case. */
      if (pCap->type == ct_Null) {
      	sched_commit_point();
	return;
      }

      Process *p = (Process *)pCap->u2.prepObj.target;

      obhdr_dirty(&p->hdr);
      sched_commit_point();

      p->state.notices |= notices;

      /* If notified process is currently receiving, wake it up. */
      if (p->state.runState == PRS_RECEIVING) {
	uintptr_t icw = get_icw(p);
	if ((icw & IPW0_CW) == 0) {
	  p->state.runState = PRS_RUNNING;
	  rq_add(&mainRQ, p, false);
	}
      }

      iParam->opw[0] = InvResult(iParam, 0);
      break;
    }

  case OC_coyotos_AppNotice_getNotices:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();

      uint32_t authorizedNotices =  iParam->iCap.cap->u1.protPayload;

      put_oparam32(iParam, authorizedNotices);
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
