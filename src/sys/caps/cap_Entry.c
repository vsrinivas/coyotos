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
#include <kerninc/StallQueue.h>
#include <kerninc/Process.h>
#include <kerninc/Sched.h>

void cap_Entry(InvParam_t *iParam)
{
  /** No opcodes are implemented by Entry Capabilities.
   *
   * The only way we can get here is if cap_prepare_for_invocation()
   * fails. The only way that can happen is if the process capability
   * stored in the Endpoint object is Null. 
   * 
   * In that case, the thing to do is to enqueue on the Endpoint
   * object.
   */

  obhdr_sleepOn(iParam->iCap.cap->u2.prepObj.target);
}
