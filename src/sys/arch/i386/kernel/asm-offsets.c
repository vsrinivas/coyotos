/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @file
 *
 * @brief Generator for assembly-language offsets against the Process
 * and register layout data structures.
 *
 * This is conceptually derived from a technique proposed by Keith
 * Owens for linux 2.5, but it shares no code.
 *
 * Note that this file is NOT linked into the Coyotos kernel. It is
 * used only to generate an intermediate assembly file, which is in
 * turn used to produce asm-offsets.h via an AWK script.
 */

#include <stddef.h>
#include <kerninc/ccs.h>
#include <kerninc/Process.h>
#include <kerninc/Endpoint.h>
#include <kerninc/GPT.h>
#include <kerninc/Depend.h>
#include "IA32/TSS.h"

// #define offsetof(ty, fld) ((size_t) &((ty *) 0)->fld)

#define ASMDEF(sym, struct, field) \
  GNU_INLINE_ASM("\n#define " #sym " %0" :: "i" (offsetof(struct, field)))

#define ASMSIZEOF(sym, ty) \
  GNU_INLINE_ASM("\n#define " #sym " %0" :: "i" (sizeof(ty)))

struct hw_trap_save {
  uint32_t vecno;
  uint32_t error;
  uint32_t eip;
  uint16_t cs;
  uint16_t    : 16;
  uint32_t eflags;

  uint32_t esp;			/* only if trap to inner ring */
  uint16_t ss;			/* only if trap to inner ring */
  uint16_t    : 16;		/* only if trap to inner ring */
};

int
main(void)
{
  ASMDEF(PR_OFF_FIXREGS, Process, state.fixregs);
  ASMDEF(PR_OFF_REENTRY_SP, Process, state.fixregs.ES);

  ASMDEF(TRAP_OFF_ERRCODE, struct hw_trap_save, error);
  ASMDEF(TRAP_OFF_EIP, struct hw_trap_save, eip);
  ASMDEF(TRAP_OFF_CS, struct hw_trap_save, cs);
  ASMDEF(TRAP_OFF_EFLAGS, struct hw_trap_save, eflags);
  ASMDEF(TRAP_OFF_ESP, struct hw_trap_save, esp);
  ASMDEF(TRAP_OFF_SS, struct hw_trap_save, ss);

  ASMDEF(PR_OFF_ISSUES, Process, issues);

  ASMDEF(PR_OFF_CS, Process, state.fixregs.CS);
  ASMDEF(PR_OFF_DS, Process, state.fixregs.DS);
  ASMDEF(PR_OFF_ES, Process, state.fixregs.ES);
  ASMDEF(PR_OFF_FS, Process, state.fixregs.FS);
  ASMDEF(PR_OFF_GS, Process, state.fixregs.GS);
  ASMDEF(PR_OFF_SS, Process, state.fixregs.SS);
  ASMDEF(PR_OFF_EAX, Process, state.fixregs.EAX);
  ASMDEF(PR_OFF_EBX, Process, state.fixregs.EBX);
  ASMDEF(PR_OFF_ECX, Process, state.fixregs.ECX);
  ASMDEF(PR_OFF_EDX, Process, state.fixregs.EDX);
  ASMDEF(PR_OFF_ESI, Process, state.fixregs.ESI);
  ASMDEF(PR_OFF_EDI, Process, state.fixregs.EDI);
  ASMDEF(PR_OFF_EBP, Process, state.fixregs.EBP);
  ASMDEF(PR_OFF_ESP, Process, state.fixregs.ESP);
  ASMDEF(PR_OFF_EIP, Process, state.fixregs.EIP);
  ASMDEF(PR_OFF_EFLAGS, Process, state.fixregs.EFLAGS);

  ASMDEF(PR_OFF_ONCPU, Process, onCPU);
  ASMDEF(CPU_OFF_STACKTOP, CPU, topOfStack);

  ASMDEF(FIX_OFF_CS, fixregs_t, CS);
  ASMDEF(FIX_OFF_DS, fixregs_t, DS);
  ASMDEF(FIX_OFF_ES, fixregs_t, ES);
  ASMDEF(FIX_OFF_FS, fixregs_t, FS);
  ASMDEF(FIX_OFF_GS, fixregs_t, GS);
  ASMDEF(FIX_OFF_SS, fixregs_t, SS);
  ASMDEF(FIX_OFF_EAX, fixregs_t, EAX);
  ASMDEF(FIX_OFF_EBX, fixregs_t, EBX);
  ASMDEF(FIX_OFF_ECX, fixregs_t, ECX);
  ASMDEF(FIX_OFF_EDX, fixregs_t, EDX);
  ASMDEF(FIX_OFF_ESI, fixregs_t, ESI);
  ASMDEF(FIX_OFF_EDI, fixregs_t, EDI);
  ASMDEF(FIX_OFF_EBP, fixregs_t, EBP);
  ASMDEF(FIX_OFF_ESP, fixregs_t, ESP);
  ASMDEF(FIX_OFF_EIP, fixregs_t, EIP);
  ASMDEF(FIX_OFF_EFLAGS, fixregs_t, EFLAGS);
  ASMDEF(FIX_OFF_ERRCODE, fixregs_t, Error);
  ASMDEF(FIX_OFF_PFADDR, fixregs_t, ExceptAddr);
  ASMDEF(FIX_OFF_VECNO, fixregs_t, ExceptNo);
  ASMDEF(FIX_OFF_REENTRY_SP, fixregs_t, ES);

  ASMDEF(TSS_RING0_SP, ia32_TSS, esp0);

  ASMSIZEOF(PROCESS_SIZE, Process);
  ASMSIZEOF(GPT_SIZE, GPT);
  ASMSIZEOF(PAGESTRUCT_SIZE, Page);
  ASMSIZEOF(ENDPOINT_SIZE, Endpoint);
  ASMSIZEOF(DEPEND_SIZE, Depend);
  ASMSIZEOF(CPU_SIZE, CPU);
  return 0;
}
