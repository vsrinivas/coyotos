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
#include <kerninc/string.h>
#include <kerninc/util.h>
#include <coyotos/syscall.h>
#include <hal/syscall.h>
#include <idl/coyotos/KernLog.h>

extern void cap_Cap(InvParam_t* iParam);

void
cap_KernLog(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  switch(opCode) {
  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();
      InvTypeMessage(iParam, IKT_coyotos_KernLog);
      return;
    }

  case OC_coyotos_KernLog_log:
    {
      uintptr_t max __attribute__((unused)) = get_iparam32(iParam);
      uintptr_t len = get_iparam32(iParam);
      uintptr_t ptr_ignored __attribute__((unused)) = 
	(sizeof(void *) == 4)
	? ((archaddr_t) get_iparam32(iParam))
	: get_iparam64(iParam);

      INV_REQUIRE_ARGS_S_M(iParam, 0, len, 256);

      uintptr_t sndptr = get_pw(iParam->invoker, IPW_SNDPTR);

      sched_commit_point();

      if (len > 255) {
	InvErrorMessage(iParam, RC_coyotos_Cap_RequestError);
	return;
      }
      else if (vm_valid_uva(iParam->invoker, sndptr) && 
	  vm_valid_uva(iParam->invoker, sndptr + len - 1)) {

	char dupStr[257];
	memcpy(dupStr, (void *) sndptr, len);
	if (len > 0 && dupStr[len-1] != '\n')
	  dupStr[len++] = '\n';
	dupStr[len] = 0;

	/** @bug This is all moderately delicate */
	printf("%s\r", dupStr);
      }
      else {
	InvErrorMessage(iParam, RC_coyotos_Cap_NoAccess);
	return;
      }

      return;
    }

  default:
    cap_Cap(iParam);
    break;
  }
}
