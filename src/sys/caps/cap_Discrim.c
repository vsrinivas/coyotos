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
#include <kerninc/Process.h>
#include <hal/syscall.h>
#include <coyotos/syscall.h>
#include <idl/coyotos/Discrim.h>

extern void cap_Cap(InvParam_t* iParam);

void cap_Discrim(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  switch(opCode) {
  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();
      InvTypeMessage(iParam, IKT_coyotos_Discrim);
      return;
    }

  case OC_coyotos_Discrim_classify:
    {
      INV_REQUIRE_ARGS(iParam, 1);

      cap_prepare(iParam->srcCap[1].cap);

      sched_commit_point();

      uintptr_t result = coyotos_Discrim_capClass_clOther;

      switch(iParam->srcCap[1].cap->type) {
      case ct_Null:
	result = coyotos_Discrim_capClass_clNull;
	break;
      case ct_Window:
      case ct_LocalWindow:
	result = coyotos_Discrim_capClass_clWindow;
	break;
      case ct_Page:
      case ct_CapPage:
      case ct_GPT:
	result = coyotos_Discrim_capClass_clMemory;
	break;
      case ct_Schedule:
	result = coyotos_Discrim_capClass_clSched;
	break;
      case ct_Endpoint:
	result = coyotos_Discrim_capClass_clEndpoint;
	break;
      case ct_Entry:
	result = coyotos_Discrim_capClass_clEntry;
	break;
      case ct_Process:
	result = coyotos_Discrim_capClass_clProcess;
	break;
      case ct_AppNotice:
	result = coyotos_Discrim_capClass_clAppNotice;
	break;
      default:
	result = coyotos_Discrim_capClass_clOther;
	break;
      }

      put_oparam32(iParam, result);
      iParam->opw[0] = InvResult(iParam, 0);

      break;
    }

  case OC_coyotos_Discrim_isDiscreet:
    {
      INV_REQUIRE_ARGS(iParam, 1);

      cap_prepare(iParam->srcCap[1].cap);

      sched_commit_point();

      bool isDiscreet = false;
      switch(iParam->srcCap[1].cap->type) {
      case ct_Null:
      case ct_Window:
      case ct_LocalWindow:
      case ct_CapBits:
      case ct_Discrim:
	isDiscreet = true;
	break;
      case ct_Page:
      case ct_CapPage:
      case ct_GPT:
	if (iParam->srcCap[1].cap->restr & CAP_RESTR_WK)
	  isDiscreet = true;
	break;
      default:
	break;
      }

      put_oparam32(iParam, isDiscreet ? 1 : 0);
      iParam->opw[0] = InvResult(iParam, 0);

      break;
    }

  case OC_coyotos_Discrim_compare:
    {
      INV_REQUIRE_ARGS(iParam, 2);

      capability *c1 = iParam->srcCap[1].cap;
      capability *c2 = iParam->srcCap[2].cap;
      cap_prepare(c1);
      cap_prepare(c2);

      sched_commit_point();

      bool same = ((c1->type == c2->type) &&
		   (c1->restr == c2->restr) &&
		   (c1->allocCount == c2->allocCount) &&

		   // Following compare covers all variants of the u1 union:
		   (c1->u1.protPayload == c2->u1.protPayload) &&
		   
		   // Following compare covers all variants of the u2 union:
		   (c1->u2.offset == c2->u2.offset));

      put_oparam32(iParam, same ? 1 : 0);
      iParam->opw[0] = InvResult(iParam, 0);

      break;
    }

  default:
    cap_Cap(iParam);
    break;
  }
}
