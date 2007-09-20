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
#include <coyotos/syscall.h>
#include <hal/syscall.h>
#include <idl/coyotos/Memory.h>

extern void cap_Cap(InvParam_t* iParam);

void
cap_Memory(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  /* This interface is abstract. */

  switch(opCode) {

  case OC_coyotos_Memory_reduce:
    {
      uint32_t restr = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      if (restr & ~CAP_RESTR_MASK) {
	sched_commit_point();
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      sched_commit_point();

      if (restr & ~CAP_RESTR_MASK) {
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }
      else if ((restr & CAP_RESTR_OP) && iParam->iCap.cap->type != ct_GPT) {
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      if (restr & CAP_RESTR_WK)
	restr |= CAP_RESTR_RO;

      cap_set(&iParam->srcCap[0].theCap, iParam->iCap.cap);
      iParam->srcCap[0].theCap.restr |= restr;
      iParam->opw[0] = InvResult(iParam, 1);
      return;
    }
  case OC_coyotos_Memory_getRestrictions:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();

      put_oparam32(iParam, iParam->iCap.cap->restr);

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }
  case OC_coyotos_Memory_getGuard:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();

      put_oparam32(iParam, make_guard(iParam->iCap.cap->u1.mem.match,
				      iParam->iCap.cap->u1.mem.l2g));

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }
  case OC_coyotos_Memory_setGuard:
    {
      uint32_t guard = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();

      uint32_t l2g = guard_l2g(guard);
      uint32_t match = guard_match(guard);

      if (!guard_valid(l2g)) {
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      iParam->opw[0] = InvResult(iParam, 1);

      cap_set(&iParam->srcCap[0].theCap, iParam->iCap.cap);
      iParam->srcCap[0].theCap.u1.mem.l2g = l2g;
      iParam->srcCap[0].theCap.u1.mem.match = match;

      return;
    }
  default:
    {
      cap_Cap(iParam);
      break;
    }
  }
}
