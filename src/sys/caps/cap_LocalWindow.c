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
#include <kerninc/util.h>
#include <coyotos/syscall.h>
#include <hal/syscall.h>
#include <idl/coyotos/LocalWindow.h>

extern void cap_Window(InvParam_t* iParam);

void
cap_LocalWindow(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  switch(opCode) {
  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();
      InvTypeMessage(iParam,  IKT_coyotos_LocalWindow);
      return;
    }

  case OC_coyotos_LocalWindow_getRootSlot:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();
      put_oparam32(iParam, iParam->iCap.cap->allocCount);
      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  default:
    cap_Window(iParam);
    break;
  }
}
