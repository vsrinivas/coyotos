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

/** @file
 *
 * @brief Data structures and initialization code for the Pentium
 * interrupt handling mechanism.
 */


#include <hal/kerntypes.h>
#include <hal/machine.h>
#include <hal/irq.h>
#include <kerninc/assert.h>
#include <kerninc/printf.h>
#include <kerninc/ccs.h>
#include <kerninc/Process.h>
#include <kerninc/Sched.h>
#include <kerninc/event.h>

#include "IA32/GDT.h"
#include "GDT.h"
#include "IRQ.h"
#include "Selector.h"
#include "hwmap.h"
#include "PIC.h"

/** @brief Hardware-level interrupt dispatch table. */
PAGE_ALIGN static ia32_GateDescriptor IdtTable[NUM_TRAP+NUM_IRQ];

static void irq_SetHardwareVector(int, void (*)(void), bool isUser);

#define DEBUG_PAGEFAULT if (0)

static void
dump_savearea(fixregs_t *regs)
{
  if (((regs->EFLAGS & EFLAGS_VM) == 0) &&
      regs->CS &&
      ((regs->CS & 0x3u) == 0x0u)) {
    /* Kernel-mode trap. */
    uint16_t ss;
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;

    GNU_INLINE_ASM("mov %%ss,%0;" : "=r" (ss));
    GNU_INLINE_ASM("mov %%ds,%0;" : "=r" (ds));
    GNU_INLINE_ASM("mov %%es,%0;" : "=r" (es));
    GNU_INLINE_ASM("mov %%fs,%0;" : "=r" (fs));
    GNU_INLINE_ASM("mov %%gs,%0;" : "=r" (gs));

    printf("CS:EIP 0x%04x:0x%08x"
	   " SS:ESP 0x%04x:0x%08x (kern 0x%08x)\n"
	   "DS  0x%04x ES  0x%04x FS  0x%04x GS 0x%04x\n"
	   "EAX 0x%08x EBX 0x%08x ECX 0x%08x EDX 0x%08x\n"
	   "EDI 0x%08x ESI 0x%08x ESP 0x%08x EBP 0x%08x\n"
	   "Vec 0x%02x Err 0x%04x ExAddr 0x%08x\n",
	   regs->CS, regs->EIP,
	   ss, ((uintptr_t) regs) + offsetof(fixregs_t, ESP), regs,
	   ds, es, fs, gs,
	   regs->EAX, regs->EBX, regs->ECX, regs->EDX,
	   regs->EDI, regs->ESI, regs->ESP, regs->EBP,
	   regs->ExceptNo, regs->Error, regs->ExceptAddr);
  }
  else
    printf("CS:EIP 0x%04x:0x%08x"
	   " SS:ESP 0x%04x:0x%08x (user 0x%08x)\n"
	   "DS  0x%04x ES  0x%04x FS  0x%04x GS 0x%04x\n"
	   "EAX 0x%08x EBX 0x%08x ECX 0x%08x EDX 0x%08x\n"
	   "EDI 0x%08x ESI 0x%08x ESP 0x%08x EBP 0x%08x\n"
	   "Vec 0x%02x Err 0x%04x ExAddr 0x%08x\n",
	   regs->CS, regs->EIP,
	   regs->SS, regs->ESP, regs,
	   regs->DS, regs->ES, regs->FS, regs->GS,
	   regs->EAX, regs->EBX, regs->ECX, regs->EDX,
	   regs->EDI, regs->ESI, regs->ESP, regs->EBP,
	   regs->ExceptNo, regs->Error, regs->ExceptAddr);

#if 0
  {
    char *comma="";
    printf("FLAGS=");
    if(regs->EFLAGS & EFLAGS_CF) {printf("%sCF",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_PF) {printf("%sPF",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_AF) {printf("%sAF",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_ZF) {printf("%sZF",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_SF) {printf("%sSF",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_TF) {printf("%sTF",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_IF) {printf("%sIF",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_DF) {printf("%sDF",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_OF) {printf("%sOF",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_IOPL) {
      printf("%sIOPL=%d",comma,(regs->eflags >> 12) & 0x3); comma=","; }
    if(regs->EFLAGS & EFLAGS_NT) {printf("%sNT",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_RF) {printf("%sRF",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_VM) {printf("%sVM",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_AC) {printf("%sAC",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_VIF) {printf("%sVIF",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_VIP) {printf("%sVIP",comma); comma=","; }
    if(regs->EFLAGS & EFLAGS_ID) {printf("%sID",comma); comma=","; }
  }
#endif
  printf("FLAGS=%s %s %s %s %s %s %s %s %s IOPL=%d  %s %s %s %s %s %s %s\n",
	 (regs->EFLAGS & EFLAGS_CF) ? "CF" : "cf",
	 (regs->EFLAGS & EFLAGS_PF) ? "PF" : "pf",
	 (regs->EFLAGS & EFLAGS_AF) ? "AF" : "af",
	 (regs->EFLAGS & EFLAGS_ZF) ? "ZF" : "zf",
	 (regs->EFLAGS & EFLAGS_SF) ? "SF" : "sf",
	 (regs->EFLAGS & EFLAGS_TF) ? "TF" : "tf",
	 (regs->EFLAGS & EFLAGS_IF) ? "IF" : "if",
	 (regs->EFLAGS & EFLAGS_DF) ? "DF" : "df",
	 (regs->EFLAGS & EFLAGS_OF) ? "OF" : "of",
	 (regs->EFLAGS >> 12) & 0x3,
	 (regs->EFLAGS & EFLAGS_NT) ? "NT" : "nt",
	 (regs->EFLAGS & EFLAGS_RF) ? "RF" : "rf",
	 (regs->EFLAGS & EFLAGS_VM) ? "VM" : "vm",
	 (regs->EFLAGS & EFLAGS_AC) ? "AC" : "ac",
	 (regs->EFLAGS & EFLAGS_VIF) ? "VIF" : "vif",
	 (regs->EFLAGS & EFLAGS_VIP) ? "VIP" : "vip",
	 (regs->EFLAGS & EFLAGS_ID) ? "ID" : "id");
}

void
proc_dump_current_savearea()
{
  dump_savearea(&MY_CPU(current)->state.fixregs);
}

static void
vh_DebugException(Process *inProc, fixregs_t *saveArea)
{
  assert(inProc);
  proc_TakeFault(inProc, coyotos_Process_FC_Debug, 0);
}


static void 
vh_BptTrap(Process *inProc, fixregs_t *saveArea)
{
  assert(inProc);
  
  inProc->state.fixregs.EIP -= 0x1;	/* correct the PC! */

  proc_TakeFault(inProc, coyotos_Process_FC_BreakPoint, 0);
}

static void
vh_FatalFault(Process *inProc, fixregs_t *saveArea)
{
  uint32_t vecno = saveArea->ExceptNo;

  switch(vecno) {
  /* Faults that are systemwide fatal */
  case vec_NMI:
    {
      fatal("Unexpected NMI\n");
    }
  case vec_DoubleFault:
    {
      fatal("Unexpected Double Fault\n");
    }
  case vec_InvalTSS:
    {
      fatal("Invalid TSS\n");
    }
  case vec_CoprocError:
    {
      fatal("Coprocessor Error\n");
    }
  case vec_MachineCheck:
    {
      fatal("Machine Check\n");
    }
  }
}

void vh_DeviceNotAvailable(Process *inProc, fixregs_t *saveArea)
{
  fatal("Device Not Available\n");
}

#define PAGEFAULT_ERROR_P   0x01  /**<@brief If clear, not-present fault. */
#define PAGEFAULT_ERROR_RW  0x02  /**<@brief If set, write attempt. */
#define PAGEFAULT_ERROR_US  0x04  /**<@brief If set, user mode. */
#define PAGEFAULT_ERROR_RSV 0x08  /**<@brief If set, reserved bits not 0. */
#define PAGEFAULT_ERROR_ID  0x10  /**<@brief If set, instruction fetch. */

static void 
vh_PageFault(Process *inProc, fixregs_t *saveArea)
{
  uintptr_t e = saveArea->Error;
  uintptr_t addr = saveArea->ExceptAddr;

  LOG_EVENT(ety_PageFault, inProc, e, addr);

  /** @bug wait;  should we be using current instead of inProc? */
  DEBUG_PAGEFAULT
    printf("Page fault trap va=0x%08p, errorCode 0x%x\n", saveArea->ExceptAddr,
	   saveArea->Error);

  if (e & PAGEFAULT_ERROR_RSV)
    fatal("PageFault due to non-zero reserved bits");

  assert((inProc == 0) == ((e & PAGEFAULT_ERROR_US) == 0));

  if (addr >= KVA && (e & PAGEFAULT_ERROR_US) == 0)
    bug("supervisor-mode pagefault at 0x%x\n", addr);

  if (inProc == 0) {
    /* We have taken a page fault from kernel mode while attempting to
       access a user-mode address. This can occur only from the
       invocation path. What we do here is attribute the page fault to
       the current process and proceed exactly as if we had faulted
       from user mode. */
    inProc = MY_CPU(current);
    assert(inProc);
    printf("User-address pagefault in supervisor mode at EIP=0x%p VA=0x%p\n",
	saveArea->EIP, addr);
    /* Enable interrupts, because we want to allow preemption during
       processing of this page fault.

       Note that this creates a possible exception nesting on the
       kernel stack:

       Top
         PageFault
           Other Interrupt
    */
    GNU_INLINE_ASM("sti");
  }

  bool wantExec = NXSupported && (e & PAGEFAULT_ERROR_ID);
  bool wantWrite = (e & PAGEFAULT_ERROR_RW);

  DEBUG_PAGEFAULT
    printf("do_pageFault(proc=0x%p, va=0x%08x, %s, %s)\n", inProc, addr,
	   wantWrite ? "wantWrite" : "0", wantExec ? "wantExec" : "0");

  do_pageFault(inProc, addr, wantWrite, wantExec);

  DEBUG_PAGEFAULT
    printf("do_pageFault(...) done\n");

  vm_switch_curcpu_to_map(inProc->mappingTableHdr);
}

static void 
vh_UserFault(Process *inProc, fixregs_t *saveArea)
{
#ifndef NDEBUG
  if (inProc == 0) {
    printf("vec_UserFault from kernel (?):\n");
    dump_savearea(saveArea);
    fatal("/* Can't Happen :-) */\n");
  }
#endif
  uint32_t vecno = saveArea->ExceptNo;

  switch(vecno) {
  case vec_DivZero:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_DivZero, 0);
    }
  case vec_Overflow:
    {
#ifdef BRING_UP
      fatal("Process took integer overflow exception\n");
#endif
      proc_TakeFault(inProc, coyotos_Process_FC_Overflow, 0);
    }
  case vec_Bounds:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_Bounds, 0);
    }
  case vec_BadOpcode:
    {
      proc_SetFault(inProc, coyotos_Process_FC_BadOpcode, 0);
    }
  case vec_GeneralProtection: 
    {
      printf("Took a %s general protection fault. Saved state is:\n",
	     inProc ? "process" : "kernel");
      dump_savearea(saveArea);
      proc_TakeFault(inProc, coyotos_Process_FC_GeneralProtection, 0);
    }
  case vec_StackSeg:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_StackSeg, 0);
    }
  case vec_SegNotPresent:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_SegNotPresent, 0);
    }
  case vec_AlignCheck:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_BadAlign, 0);
    }
  case vec_SIMDfp:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_SIMDfp, 0);
    }
  }
}

static void 
vh_ReservedException(Process *inProc, fixregs_t *saveArea)
{
  fatal("Reserved exception 0x%x\n", saveArea->ExceptNo);
}

static void
vh_LegacySyscall(Process *inProc, fixregs_t *saveArea)
{
  fatal("Legacy syscall\n");
}

void
vh_SysCall(Process *inProc, fixregs_t *saveArea)
{
  assert(inProc);

  /* Load the PC and SP values from their source registers first.  This
   * matches the behavior of SYSCALL/SYSENTER.
   */
  inProc->state.fixregs.ESP = inProc->state.fixregs.ECX;
  inProc->state.fixregs.EIP = inProc->state.fixregs.EDX;

  proc_syscall();

  if (inProc->issues & pi_SysCallDone)
    inProc->state.fixregs.EIP += 2;
}

/** @brief Handler function for unbound interrupts. */
static void
vh_UnboundIRQ(Process *inProc, fixregs_t *saveArea)
{
  fatal("Received orphaned %s interrupt %d [err=0x%x]\n", 
      inProc ? "process" : "kernel",
      VectorMap[saveArea->ExceptNo].irqSource, saveArea->Error);
}

/** @brief Reverse map for finding vectors from IRQ numbers */
uint8_t IrqVector[NUM_IRQ];

/** @brief Software-level interrupt dispatch mechanism. */
struct VectorInfo VectorMap[NUM_VECTOR] = {
  /* 0x00 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UserFault
	       /* Divide by Zero */ },
  /* 0x01 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_DebugException 
	       /* Debug */ },
  /* 0x02 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_FatalFault
	       /* NMI */},
  /* 0x03 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=1,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_BptTrap
	       /* Breakpoint */},
  /* 0x04 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=1,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UserFault 
	       /* Overflow */},
  /* 0x05 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=1,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UserFault 
	       /* Bounds Error */ },
  /* 0x06 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UserFault 
	       /* Bad Opcode */ },
  /* 0x07 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_DeviceNotAvailable
	       /* Device Not Available */ },
  /* 0x08 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_FatalFault
	       /* Double Fault */ },
  /* 0x09 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ /* ????? */
	       /* Coprocessor Overrun */ },
  /* 0x0a */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_FatalFault
	       /* Invalid TSS */ },
  /* 0x0b */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UserFault 
	       /* Segment Not Present */ },
  /* 0x0c */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UserFault 
	       /* Stack Segment Error */ },
  /* 0x0d */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UserFault 
	       /* General Protection Violation */ },
  /* 0x0e */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_PageFault 
	       /* Page Fault */ },
  /* 0x0f */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved) */ },
  /* 0x10 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_FatalFault
	       /* Coprocessor Error */ },
  /* 0x11 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UserFault 
	       /* Alignment Check */ },
  /* 0x12 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_FatalFault
	       /* Machine Check */},
  /* 0x13 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UserFault 
	       /* SIMD Floating Point error */ },
  /* 0x14 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException 
	       /* (undefined, reserved */},
  /* 0x15 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved */},
  /* 0x16 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved */},
  /* 0x17 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved */},
  /* 0x18 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved */},
  /* 0x19 */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved */},
  /* 0x1a */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved */},
  /* 0x1b */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved */},
  /* 0x1c */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved */},
  /* 0x1d */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved */},
  /* 0x1e */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved */},
  /* 0x1f */ { .count = 0, .type = vt_HardTrap, .enabled = 1, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_ReservedException
	       /* (undefined, reserved */},
  /* START OF LEGACY PIC INTERRUPTS
   *
   * These are edge triggered until proven otherwise
   */
  /* 0x20 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ
	       /* IRQ0 (Legacy PIT) */ },
  /* 0x21 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ 
	       /* IRQ1 (Legacy Keyboard) */ },
  /* 0x22 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ
	       /* IRQ2 (8259 cascade -- should never happen) */ },
  /* 0x23 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x24 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x25 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x26 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x27 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x28 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x29 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x2a */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x2b */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x2c */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x2d */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x2e */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x2f */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 1, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* END OF LEGACY PIC INTERRUPTS */
  /* 0x30 */ { .count = 0, .type = vt_SysCall, .enabled = 0, .user=1,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_SysCall },
  /* 0x31 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x32 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x33 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x34 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x35 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x36 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x37 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x38 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x39 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x3a */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x3b */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x3c */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x3d */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x3e */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x3f */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x40 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x41 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x42 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x43 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x44 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x45 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x46 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x47 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x48 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x49 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x4a */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x4b */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x4c */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x4d */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x4e */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x4f */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x50 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x51 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x52 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x53 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x54 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x55 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x56 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x57 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x58 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x59 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x5a */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x5b */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x5c */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x5d */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x5e */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x5f */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x60 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x61 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x62 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x63 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x64 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x65 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x66 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x67 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x68 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x69 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x6a */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x6b */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x6c */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x6d */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x6e */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x6f */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x70 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x71 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x72 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x73 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x74 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x75 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x76 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x77 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x78 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x79 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x7a */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x7b */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x7c */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x7d */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x7e */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x7f */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x80 */ { .count = 0, .type = vt_SysCall,   .enabled = 0, .user=1,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_LegacySyscall },
  /* 0x81 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x82 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x83 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x84 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x85 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x86 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x87 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x88 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x89 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x8a */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x8b */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x8c */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x8d */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x8e */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x8f */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x90 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x91 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x92 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x93 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x94 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x95 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x96 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x97 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x98 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x99 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x9a */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x9b */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x9c */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x9d */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x9e */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0x9f */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xa0 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xa1 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xa2 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xa3 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xa4 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xa5 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xa6 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xa7 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xa8 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xa9 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xaa */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xab */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xac */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xad */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xae */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xaf */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xb0 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xb1 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xb2 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xb3 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xb4 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xb5 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xb6 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xb7 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xb8 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xb9 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xba */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xbb */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xbc */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xbd */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xbe */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xbf */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xc0 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xc1 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xc2 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xc3 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xc4 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xc5 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xc6 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xc7 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xc8 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xc9 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xca */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xcb */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xcc */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xcd */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xce */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xcf */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xd0 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xd1 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xd2 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xd3 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xd4 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xd5 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xd6 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xd7 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xd8 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xd9 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xda */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xdb */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xdc */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xdd */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xde */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xdf */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xe0 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xe1 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xe2 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xe3 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xe4 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xe5 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xe6 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xe7 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xe8 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xe9 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xea */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xeb */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xec */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xed */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xee */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xef */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xf0 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xf1 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xf2 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xf3 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xf4 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xf5 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xf6 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xf7 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xf8 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xf9 */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xfa */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xfb */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xfc */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xfd */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
  /* 0xfe */ { .count = 0, .type = vt_Interrupt, .enabled = 0, .user=0,
	       .edge = 0, .irqSource = VECTOR_NO_IRQ, .fn = vh_UnboundIRQ },
};
// static uint32_t NestedInterrupts[(NUM_TRAP+NUM_IRQ+31)/32];

// const char *hex = "0123456789abcdefg";

// SIMD floating point not handled

void
irq_OnTrapOrInterrupt(Process *inProc, fixregs_t *saveArea)
{
  LOG_EVENT(ety_Trap, inProc, saveArea->ExceptNo, saveArea->Error);

  if (inProc) {
    MY_CPU(curCPU)->hasPreempted = false;
    GNU_INLINE_ASM("sti");
  }

  // printf("OnTrapOrInterrupt with vector %d\n", vecno);

  uint32_t vecno = saveArea->ExceptNo;

  if (VectorMap[vecno].type == vt_Interrupt) { /* hardware interupt */
    uint32_t irq = VectorMap[vecno].irqSource;
    /* The i8259 has ringing problems on IRQ7 and IRQ15.  APICs have
     * race conditions in obscure cases.
     *
     * Rather than wake up user-mode drivers for interrupts that are
     * instantaneously de-asserted, check here whether the interrupt
     * that we allegedly received is still pending. If not, simply
     * acknowledge the PIC and return to whatever we were doing.
     */
    if ( !pic_isPending(irq) ) {
      printf("IRQ %d no longer pending\n", irq);
      pic_acknowledge(irq);

      return;
    }

    /* Interrupt actually appears to be pending. This is good. Disable
     * the interrupt at the PIC and acknowledge to the PIC that we
     * have taken responsibility for it.
     */
    /// @bug IOAPIC requires a late acknowledge rather than an early
    /// acknowledge. I do not understand their theory of operation
    /// well enough to do anything sensible about them here. Need to
    /// read up on that.
    pic_disable(irq);
    pic_acknowledge(irq);
  }

  if (inProc)
    mutex_grab(&inProc->hdr.lock);

  VectorMap[vecno].count++;
  VectorMap[vecno].fn(inProc, saveArea);

  /** @bug Some interrupts require late acknowledge. That may need to
   * be added here when APIC stuff is done. */

  /* We do NOT re-enable the taken interrupt on the way out. That is
   * the driver's job to do if it wants to do it.
   */

  if (inProc) {
#if 0
    /* If the current process has any reasons that it cannot run,
     * dispatch this process through the long dispatch path that knows
     * how to deal with all of the ugly cases.
     */
    if (MY_CPU(current) && MY_CPU(current)->issues)
      proc_dispatch_current();

    /* Must gang-release any remaining process locks before exiting
       kernel. */
    mutex_release_all_process_locks();

    GNU_INLINE_ASM("cli");
    MY_CPU(curCPU)->hasPreempted = false;
#else
    assert(MY_CPU(current));
    proc_dispatch_current();
#endif
  }

  return;
}

/** @brief Set hardware interrupt vector entry to known value. */
void
irq_SetHardwareVector(int entry, void (*procPtr)(void), bool allowUser)
{
  uint32_t wProcPtr = (uint32_t) procPtr;

  IdtTable[entry].loOffset = (uint16_t) wProcPtr;
  IdtTable[entry].selector = sel_KernelCode;
  IdtTable[entry].zero = 0;
  IdtTable[entry].type = 0xeu;
  IdtTable[entry].system = 0;
  /* Use RPL==1 for non-user so kernel threads can call them. */
  IdtTable[entry].dpl = allowUser ? 3 : 1;
  IdtTable[entry].present = 1;
  IdtTable[entry].hiOffset = (uint16_t) (wProcPtr >> 16);
}

typedef void (*TrapStub)();
/** @brief Stub table defined in interrupt.S */
extern TrapStub irq_stubs[NUM_VECTOR];

typedef struct {
  uint16_t size;
  uint32_t lowAddr __attribute((packed));
  uint16_t :16;			/* pad */
} DescriptorTablePointer;

/** @brief Initialize the interrupt descriptor table.
 */
void irq_vector_init()
{
  for (size_t irq = 0; irq < NUM_IRQ; irq++)
    IrqVector[irq] = IRQ_NO_VECTOR;

  /* Initialize the vector to interrupt source mapping. Doing it this
     way reduces errors when updating the table. */
  {
    uint32_t nextInt = 0;
    for (uint32_t vec = 0; vec < NUM_VECTOR; vec++) {
      if (VectorMap[vec].type == vt_Interrupt) {
	VectorMap[vec].irqSource = nextInt;
	nextInt++;
      }
    }
  }

  /* Initialize all hardware table entries. */
  for (uint32_t vec = 0; vec < NUM_VECTOR; vec++) {
    IdtTable[vec].present = 0;
    bool isUser = VectorMap[vec].user ? true : false;
    irq_SetHardwareVector(vec, irq_stubs[vec], isUser);

    if (VectorMap[vec].type == vt_Interrupt)
      IrqVector[VectorMap[vec].irqSource] = vec;
  }

  DescriptorTablePointer IDTdescriptor = {
    sizeof(IdtTable),
    (uint32_t) IdtTable
  };

  GNU_INLINE_ASM("lidt %0"
		 : /* no output */
		 : "m" (IDTdescriptor) 
		 : "memory");
}

/** @brief Initialize the IDTR on the current CPU.
 *
 * Also sets up the interrupt controllers.
 */
void irq_init()
{
  pic_init();

  DescriptorTablePointer IDTdescriptor = {
    sizeof(IdtTable),
    (uint32_t) IdtTable 
  };



  GNU_INLINE_ASM("lidt %0"
		 : /* no output */
		 : "m" (IDTdescriptor) 
		 : "memory");

  printf("IDT");
}

void irq_DoTripleFault()
{
  DescriptorTablePointer IDTdescriptor = {
    0,
    0
  };

  GNU_INLINE_ASM("lidt %0"
		 : /* no output */
		 : "m" (IDTdescriptor) 
		 : "memory");
  GNU_INLINE_ASM ("int3");

  sysctl_halt();
}


static spinlock_t irq_vector_spinlock;

/** @brief Set up the software vector table to point to the provided
    handler procedure. */
void
irq_BindInterrupt(uint32_t irq, VecFn fn)
{
  uint32_t vector = IrqVector[irq];
  assert(vector);

  SpinHoldInfo shi = spinlock_grab(&irq_vector_spinlock);

  VectorMap[vector].count = 0;
  VectorMap[vector].fn = fn;

  spinlock_release(shi);
}

/** @brief Set up the software vector table to point to the provided
    handler procedure. */
void
irq_UnbindVector(uint32_t irq)
{
  uint32_t vector = IrqVector[irq];
  assert(vector);

  SpinHoldInfo shi = spinlock_grab(&irq_vector_spinlock);

  VectorMap[vector].count = 0;
  VectorMap[vector].fn = vh_UnboundIRQ;

  spinlock_release(shi);
}

void
irq_EnableInterrupt(uint32_t irq)
{
  uint32_t vector = IrqVector[irq];
  assert(vector);

  pic_enable(irq);
  VectorMap[vector].enabled = 1;
}
