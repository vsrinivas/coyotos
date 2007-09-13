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

/** @brief Software-level interrupt dispatch mechanism. */
struct IntVecInfo IntVecEntry[NUM_TRAP+NUM_IRQ];
// static uint32_t NestedInterrupts[(NUM_TRAP+NUM_IRQ+31)/32];

const char *hex = "0123456789abcdefg";
#define OOPS(s) printf("%s trap\n", s)

#define VECNO(n) printf("%x interrupt\n", n)

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
irq_UserFault(Process *inProc, fixregs_t *saveArea)
{
#ifndef NDEBUG
  if (inProc == 0) {
    printf("irq_UserFault from kernel (?):\n");
    dump_savearea(saveArea);
    fatal("/* Can't Happen :-) */\n");
  }
#endif
  uint32_t vecno = saveArea->ExceptNo;

  switch(vecno) {
  case iv_DivZero:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_DivZero, 0);
    }
  case iv_Overflow:
    {
#ifdef BRING_UP
      fatal("Process took integer overflow exception\n");
#endif
      proc_TakeFault(inProc, coyotos_Process_FC_Overflow, 0);
    }
  case iv_Bounds:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_Bounds, 0);
    }
  case iv_BadOpcode:
    {
      proc_SetFault(inProc, coyotos_Process_FC_BadOpcode, 0);
    }
  case iv_GeneralProtection: 
    {
      printf("Took a %s general protection fault. Saved state is:\n",
	     inProc ? "process" : "kernel");
      dump_savearea(saveArea);
      proc_TakeFault(inProc, coyotos_Process_FC_GeneralProtection, 0);
    }
  case iv_StackSeg:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_StackSeg, 0);
    }
  case iv_SegNotPresent:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_SegNotPresent, 0);
    }
  case iv_AlignCheck:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_BadAlign, 0);
    }
  case iv_SIMDfp:
    {
      proc_TakeFault(inProc, coyotos_Process_FC_SIMDfp, 0);
    }
  }
}

static void irq_FatalFault(Process *inProc, fixregs_t *saveArea)
{
  uint32_t vecno = saveArea->ExceptNo;

  switch(vecno) {
  /* Faults that are systemwide fatal */
  case iv_NMI:
    {
      OOPS("II");
      sysctl_halt();
    }
  case iv_DoubleFault:
    {
      OOPS("DF");
      sysctl_halt();
    }
  case iv_InvalTSS:
    {
      OOPS("TS");
      sysctl_halt();
    }
  case iv_CoprocError:
    {
      OOPS("MF");
      sysctl_halt();
    }
  case iv_MachineCheck:
    {
      OOPS("MC");
      sysctl_halt();
    }
  }
}

void DebugException(Process *inProc, fixregs_t *saveArea)
{
  assert(inProc);
  proc_TakeFault(inProc, coyotos_Process_FC_Debug, 0);
}
void BptTrap(Process *inProc, fixregs_t *saveArea)
{
  assert(inProc);
  
  inProc->state.fixregs.EIP -= 0x1;	/* correct the PC! */

  proc_TakeFault(inProc, coyotos_Process_FC_BreakPoint, 0);
}

#define PAGEFAULT_ERROR_P   0x01  /**<@brief If clear, not-present fault. */
#define PAGEFAULT_ERROR_RW  0x02  /**<@brief If set, write attempt. */
#define PAGEFAULT_ERROR_US  0x04  /**<@brief If set, user mode. */
#define PAGEFAULT_ERROR_RSV 0x08  /**<@brief If set, reserved bits not 0. */
#define PAGEFAULT_ERROR_ID  0x10  /**<@brief If set, instruction fetch. */

void PageFault(Process *inProc, fixregs_t *saveArea)
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

void DeviceNotAvailable(Process *inProc, fixregs_t *saveArea)
{
  OOPS("NM");
  sysctl_halt();
}

// SIMD floating point not handled

void ReservedException(Process *inProc, fixregs_t *saveArea)
{
  OOPS("RZ");
  sysctl_halt();
}

void PseudoInstruction(Process *inProc, fixregs_t *saveArea)
{
  OOPS("PI");
  sysctl_halt();
}

void LegacySyscall(Process *inProc, fixregs_t *saveArea)
{
  OOPS("SC");
  sysctl_halt();
}

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

  if (IntVecEntry[vecno].flags & ivf_hardTrap) { /* hardware interupt */
    /* The i8259 has ringing problems on IRQ7 and IRQ15.  APICs have
     * race conditions in obscure cases.
     *
     * Rather than wake up user-mode drivers for interrupts that are
     * instantaneously de-asserted, check here whether the interrupt
     * that we allegedly received is still pending. If not, simply
     * acknowledge the PIC and return to whatever we were doing.
     */
    if ( !pic_isPending(vecno) ) {
      printf("Vector %d no longer pending\n", vecno);
      pic_acknowledge(vecno);

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
    pic_disable(vecno);
    pic_acknowledge(vecno);
  }

  if (inProc)
    mutex_grab(&inProc->hdr.lock);

  IntVecEntry[vecno].count++;
  IntVecEntry[vecno].fn(inProc, saveArea);

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

void
softint_SysCall(Process *inProc, fixregs_t *saveArea)
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
void
irq_UnboundVector(Process *inProc, fixregs_t *saveArea)
{
  fatal("Received orphaned %s vector %d [err=0x%x]\n", 
      inProc ? "process" : "kernel",
      saveArea->ExceptNo, saveArea->Error);

  OOPS("?V");
  VECNO(saveArea->ExceptNo);
  sysctl_halt();
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
extern TrapStub irq_stubs[NUM_TRAP+NUM_IRQ];

typedef struct {
  uint16_t size;
  uint32_t lowAddr __attribute((packed));
  uint16_t :16;			/* pad */
} DescriptorTablePointer;

/** @brief Initialize the interrupt descriptor table.
 */
void irq_vector_init()
{
  /* Initialize all hardware table entries. */
  for (uint32_t vec = 0; vec < (NUM_TRAP+NUM_IRQ); vec++) {
    IdtTable[vec].present = 0;
    irq_SetHardwareVector(vec, irq_stubs[vec], false);
  }

  /* Initialize all software entries for traps and syscalls to
     UnboundVector. */
  for (uint32_t trap = 0; trap < NUM_TRAP + NUM_IRQ; trap++)
    irq_BindVector(trap, irq_UnboundVector);
  
  /* Label the hardware interrupt vectors: */
  for (uint32_t vec = NUM_TRAP; vec < (NUM_TRAP+NUM_IRQ); vec++)
    IntVecEntry[vec].flags |= ivf_hardTrap;

  /* Undo the exceptions: */
  IntVecEntry[iv_Syscall].flags &= ~ivf_hardTrap;
  IntVecEntry[iv_LegacySyscall].flags &= ~ivf_hardTrap;

  /* Initialize software entries for system calls and make the
     corresponding hardware vectors user callable: */
  irq_BindVector(iv_Syscall, softint_SysCall);
  irq_SetHardwareVector(iv_Syscall, irq_stubs[iv_Syscall], true);

  /* Hand-wire the processor-generated exceptions. */

  /* Generic faults that can come from user land: */
  irq_BindVector(iv_DivZero, irq_UserFault);
  irq_BindVector(iv_BreakPoint, BptTrap);
  irq_BindVector(iv_Overflow, irq_UserFault);
  irq_BindVector(iv_Bounds, irq_UserFault);
  irq_BindVector(iv_BadOpcode, irq_UserFault);
  irq_BindVector(iv_GeneralProtection, irq_UserFault);
  irq_BindVector(iv_StackSeg, irq_UserFault);
  irq_BindVector(iv_SegNotPresent, irq_UserFault);
  irq_BindVector(iv_AlignCheck, irq_UserFault);
  irq_BindVector(iv_SIMDfp, irq_UserFault);

  /* Faults that require special handling */
  irq_SetHardwareVector(iv_BreakPoint, irq_stubs[iv_BreakPoint], true);
  irq_SetHardwareVector(iv_Overflow, irq_stubs[iv_Overflow], true);
  irq_SetHardwareVector(iv_Bounds, irq_stubs[iv_Bounds], true);

  irq_BindVector(iv_PageFault, PageFault);
  irq_BindVector(iv_Debug, DebugException);
  irq_BindVector(iv_DeviceNotAvail, DeviceNotAvailable);

  /* Faults that are systemwide fatal */
  irq_BindVector(iv_NMI, irq_FatalFault);
  irq_BindVector(iv_DoubleFault, irq_FatalFault);
  irq_BindVector(iv_InvalTSS, irq_FatalFault);
  irq_BindVector(iv_CoprocError, irq_FatalFault);
  irq_BindVector(iv_MachineCheck, irq_FatalFault);

  irq_BindVector(iv_LegacySyscall, LegacySyscall);

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
irq_BindVector(uint32_t vector, IrqVecFn fn)
{
  SpinHoldInfo shi = spinlock_grab(&irq_vector_spinlock);

  IntVecEntry[vector].count = 0;
  IntVecEntry[vector].fn = fn;

  spinlock_release(shi);
}

/** @brief Set up the software vector table to point to the provided
    handler procedure. */
void
irq_UnbindVector(uint32_t vector)
{
  SpinHoldInfo shi = spinlock_grab(&irq_vector_spinlock);

  IntVecEntry[vector].count = 0;
  IntVecEntry[vector].fn = irq_UnboundVector;

  spinlock_release(shi);
}

void
irq_EnableVector(uint32_t vector)
{
  assert(vector >= NUM_TRAP);
  pic_enable(vector);
}
