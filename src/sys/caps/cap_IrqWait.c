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
#include <kerninc/printf.h>
#include <kerninc/vector.h>
#include <hal/syscall.h>
#include <coyotos/syscall.h>
#include <idl/coyotos/IrqWait.h>

extern void cap_Cap(InvParam_t* iParam);

void cap_IrqWait(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  switch(opCode) {
  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();
      InvTypeMessage(iParam, IKT_coyotos_IrqWait);
      break;
    }

  case OC_coyotos_IrqWait_wait:
    {
      uint32_t irq = iParam->iCap.cap->u1.protPayload;

      INV_REQUIRE_ARGS(iParam, 0);

      VectorInfo *vector = irq_MapInterrupt(irq);
      VectorHoldInfo vhi = vector_grab(vector);

      if (vector->pending == 0) {
	sq_EnqueueOn(&vector->stallQ);

	if (!vector->unmasked) {
	  vector->unmasked = 1;
	  vector->ctrlr->unmask(vector);
	}

	vector_release(vhi);
	sched_abandon_transaction();
      }

      // sched_commit_point() cannot yield in this case, because no
      // capabilities and no strings are involved in this
      // operation. It is therefore safe to hold the vector lock
      // across the call to sched_commit_point().
      //
      // That is good, because releasing the vector lock would put us
      // in a race with another waiting party to clear the pending
      // bit.

      sched_commit_point();
      
      vector->pending = 0;
      vector_release(vhi);

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_IrqWait_disable:
    {
      uint32_t irq = iParam->iCap.cap->u1.protPayload;

      INV_REQUIRE_ARGS(iParam, 0);
	
      sched_commit_point();

      irq_DisableVector(irq);

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_IrqWait_enable:
    {
      uint32_t irq = iParam->iCap.cap->u1.protPayload;

      INV_REQUIRE_ARGS(iParam, 0);
	
      sched_commit_point();

      irq_EnableVector(irq);

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  default:
    cap_Cap(iParam);
    break;
  }
}
