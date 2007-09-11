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
#include <kerninc/string.h>
#include <kerninc/pstring.h>
#include <kerninc/util.h>
#include <coyotos/syscall.h>
#include <hal/syscall.h>
#include <idl/coyotos/i386/Process.h>

extern void cap_ProcessCommon(InvParam_t* iParam);

void cap_Process(InvParam_t *iParam)
{
  uintptr_t opCode = iParam->opCode;

  Process *p = (Process *) iParam->iCap.cap->u2.prepObj.target;

  if ((opCode != OC_coyotos_Cap_getType) && 
      (iParam->iCap.cap->restr & CAP_RESTR_RESTART)) {
    sched_commit_point();
    InvErrorMessage(iParam, RC_coyotos_Cap_NoAccess);
    return;
  }

  switch(opCode) {

  case OC_coyotos_Cap_getType:	/* Must override. */
    {
      INV_REQUIRE_ARGS(iParam, 0);

      sched_commit_point();

      InvTypeMessage(iParam, IKT_coyotos_i386_Process);
      break;
    }

  case OC_coyotos_i386_Process_getFixRegs:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      proc_ensure_exclusive(p);

      uint32_t rbound = get_pw(iParam->invokee, IPW_RCVBOUND);
      uintptr_t outVA = get_pw(iParam->invokee, IPW_RCVPTR);
      uintptr_t curVA = outVA;

      uint8_t *regs_ptr = (uint8_t *) &p->state.fixregs;

      size_t nBytes = min(sizeof(p->state.fixregs), rbound);
      size_t progress = 0;

      while (progress < nBytes) {
	struct FoundPage fp;
	coyotos_Process_FC fc = 
	  proc_findDataPage(iParam->invokee, curVA & ~COYOTOS_PAGE_ADDR_MASK,
			    true, true, &fp);

	if (fc) {
	  /* Set output length to actual bytes transferred. */
	  uintptr_t oicw = get_icw(iParam->invokee);
	  oicw |= IPW0_NB;
	  set_icw(iParam->invokee, oicw);
	  nBytes = curVA - outVA;
	  break;
	}
	obhdr_dirty(&fp.pgHdr->mhdr.hdr);

	size_t curBytes = align_up(curVA, COYOTOS_PAGE_SIZE) - curVA;
	if (curBytes == 0)
	  curBytes = COYOTOS_PAGE_SIZE;
	curBytes = min(curBytes, nBytes);

	memcpy_vtop(fp.pgHdr->pa, regs_ptr + progress, curBytes);
	progress += curBytes;
	curVA += curBytes;
      }

      set_pw(iParam->invokee, OPW_SNDLEN, nBytes);

      sched_commit_point();
      iParam->opw[0] = InvResult(iParam, 0);
      break;
    }
  case OC_coyotos_i386_Process_setFixRegs:
    {
      INV_REQUIRE_ARGS_S(iParam, 0, sizeof(p->state.fixregs));

      // Input string is known to fall within a valid address rangee
      // from earlier validation, but we need to be a little defensive
      // here, because a partial update to a register set would be
      // more than a little unpleasant.

      proc_ensure_exclusive(p);

      void *inVA = (void *) get_pw(iParam->invokee, IPW_SNDPTR);

      // Make a temporary copy in case we page fault in mid-transfer.
      fixregs_t tmp_fix;
      memcpy(&tmp_fix, inVA, sizeof(tmp_fix));

      obhdr_dirty(&p->hdr);

      sched_commit_point();

      memcpy(&p->state.fixregs, &tmp_fix, sizeof(p->state.fixregs));

      iParam->opw[0] = InvResult(iParam, 0);
      break;
    }
  case OC_coyotos_i386_Process_getFloatRegs:
    {
      INV_REQUIRE_ARGS(iParam, 0);

      proc_ensure_exclusive(p);

      uint32_t rbound = get_pw(iParam->invokee, IPW_RCVBOUND);
      uintptr_t outVA = get_pw(iParam->invokee, IPW_RCVPTR);
      uintptr_t curVA = outVA;

      uint8_t *regs_ptr = (uint8_t *) &p->state.floatregs;

      size_t nBytes = min(sizeof(p->state.floatregs), rbound);
      size_t progress = 0;

      uintptr_t opw0 = InvResult(iParam, 0);

      while (progress < nBytes) {
	struct FoundPage fp;
	coyotos_Process_FC fc = 
	  proc_findDataPage(iParam->invokee, curVA & ~COYOTOS_PAGE_ADDR_MASK,
			    true, true, &fp);

	if (fc) {
	  /* Set output length to actual bytes transferred. */
	  opw0 |= IPW0_NB;
	  nBytes = curVA - outVA;
	  break;
	}
	obhdr_dirty(&fp.pgHdr->mhdr.hdr);

	size_t curBytes = align_up(curVA, COYOTOS_PAGE_SIZE) - curVA;
	if (curBytes == 0)
	  curBytes = COYOTOS_PAGE_SIZE;
	curBytes = min(curBytes, nBytes);

	memcpy_vtop(fp.pgHdr->pa, regs_ptr + progress, curBytes);
	progress += curBytes;
	curVA += curBytes;
      }

      set_pw(iParam->invokee, OPW_SNDLEN, nBytes);

      sched_commit_point();

      break;
    }
  case OC_coyotos_i386_Process_setFloatRegs:
    {
      INV_REQUIRE_ARGS_S(iParam, 0, sizeof(p->state.floatregs));

      proc_ensure_exclusive(p);
      // Input string is known to fall within a valid address rangee
      // from earlier validation, but we need to be a little defensive
      // here, because a partial update to a register set would be
      // more than a little unpleasant.

      void *inVA = (void *) get_pw(iParam->invokee, IPW_SNDPTR);

      // Make a temporary copy in case we page fault in mid-transfer.
      floatregs_t tmp_fp;
      memcpy(&tmp_fp, inVA, sizeof(tmp_fp));

      obhdr_dirty(&p->hdr);

      sched_commit_point();

      memcpy(&p->state.floatregs, &tmp_fp, sizeof(p->state.floatregs));

      iParam->opw[0] = InvResult(iParam, 0);
      break;
    }
  default:
    {
      cap_ProcessCommon(iParam);
      break;
    }
  }
}
