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
#include <kerninc/printf.h>
#include <hal/syscall.h>
#include <coyotos/syscall.h>
#include <idl/coyotos/Sleep.h>

extern void cap_Cap(InvParam_t* iParam);

// Temporary hack:
const uint64_t restart_epoch = 1;

void cap_Sleep(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  switch(opCode) {
  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();
      InvTypeMessage(iParam, IKT_coyotos_Sleep);
      break;
    }
  case OC_coyotos_Sleep_sleepTill:
    {
      iParam->invoker->wakeTime.epoch = now.epoch;
      iParam->invoker->wakeTime.sec = get_iparam32(iParam);
      iParam->invoker->wakeTime.usec = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      if (iParam->invoker->wakeTime.usec >= 1000000) {
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      interval_delay(iParam->invoker);

      sched_commit_point();

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  case OC_coyotos_Sleep_sleepFor:
    {
      iParam->invoker->wakeTime.epoch = now.epoch;
      iParam->invoker->wakeTime.sec = get_iparam32(iParam);
      iParam->invoker->wakeTime.usec = get_iparam32(iParam);

      INV_REQUIRE_ARGS(iParam, 0);

      if (iParam->invoker->wakeTime.usec >= 1000000) {
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }

      iParam->invoker->wakeTime.sec += now.sec;
      iParam->invoker->wakeTime.usec += now.usec;
      if (iParam->invoker->wakeTime.usec >= 1000000) {
	iParam->invoker->wakeTime.usec -= 10000000;
	iParam->invoker->wakeTime.sec ++;
      }

      /* In case this restarts for some reason: */
      set_pw(iParam->invoker, 1, OC_coyotos_Sleep_sleepTill);

      interval_delay(iParam->invoker);

      sched_commit_point();

      iParam->opw[0] = InvResult(iParam, 0);
      return;
    }

  default:
    bug("Unimplemented cap type %d\n", iParam->iCap.cap->type);

    cap_Cap(iParam);
    break;
  }
}
