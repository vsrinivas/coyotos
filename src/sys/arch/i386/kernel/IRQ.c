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
#include <kerninc/vector.h>

#include "IA32/GDT.h"
#include "TSS.h"
#include "GDT.h"
#include "IRQ.h"
#include "Selector.h"
#include "hwmap.h"
#include "PIC.h"
#include "acpi.h"

/** @brief Number of interrupt sources */
irq_t nGlobalIRQ;


/** @brief Hardware-level interrupt dispatch table. */
PAGE_ALIGN static ia32_GateDescriptor IdtTable[NUM_VECTOR];

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
  dump_savearea(&MY_CPU(current)->state.regs.fix);
}

static void
vh_DebugException(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
{
  assert(inProc);
  proc_TakeFault(inProc, coyotos_Process_FC_Debug, 0);
}


static void 
vh_BptTrap(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
{
  assert(inProc);
  
  inProc->state.regs.fix.EIP -= 0x1;	/* correct the PC! */

  proc_TakeFault(inProc, coyotos_Process_FC_BreakPoint, 0);
}

static void
vh_FatalFault(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
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

void vh_DeviceNotAvailable(VectorInfo *vec, 
			   Process *inProc, fixregs_t *saveArea)
{
  fatal("Device Not Available\n");
}

#define PAGEFAULT_ERROR_P   0x01  /**<@brief If clear, not-present fault. */
#define PAGEFAULT_ERROR_RW  0x02  /**<@brief If set, write attempt. */
#define PAGEFAULT_ERROR_US  0x04  /**<@brief If set, user mode. */
#define PAGEFAULT_ERROR_RSV 0x08  /**<@brief If set, reserved bits not 0. */
#define PAGEFAULT_ERROR_ID  0x10  /**<@brief If set, instruction fetch. */

static void 
vh_PageFault(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
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

  bool wantExec = IA32_NXSupported && (e & PAGEFAULT_ERROR_ID);
  bool wantWrite = (e & PAGEFAULT_ERROR_RW);

  DEBUG_PAGEFAULT
    printf("do_pageFault(proc=0x%p, va=0x%08x, %s, %s, false)\n", inProc, addr,
	   wantWrite ? "wantWrite" : "0", wantExec ? "wantExec" : "0");

  do_pageFault(inProc, addr, wantWrite, wantExec, false);

  DEBUG_PAGEFAULT
    printf("do_pageFault(...) done\n");

  vm_switch_curcpu_to_map(inProc->mappingTableHdr);
}

static void 
vh_UserFault(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
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
      /* If this is GP(0), and the domain holds the DevicePrivs key in a
       * register, but it's privilege level is not appropriate for I/O
       * access, escalate it's privilege level. */

      if (inProc) {
	uint32_t iopl = saveArea->EFLAGS & EFLAGS_IOPL;
	if (saveArea->Error == 0 
	    && iopl < EFLAGS_IOPL3
	    && inProc->state.ioSpace.type == ct_IOPriv) {
	  // No need to mask out old value, since we are writing all
	  // 1's to the field:
	  inProc->state.regs.fix.EFLAGS |= EFLAGS_IOPL3;
	  return;
	}
      }

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
vh_ReservedException(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
{
  fatal("Reserved exception 0x%x\n", saveArea->ExceptNo);
}

void
vh_SysCall(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
{
  assert(inProc);

  /* Load the PC and SP values from their source registers first.  This
   * matches the behavior of SYSCALL/SYSENTER.
   */
  inProc->state.regs.fix.ESP = inProc->state.regs.fix.ECX;
  inProc->state.regs.fix.EIP = inProc->state.regs.fix.EDX;

  proc_syscall();

  if (atomic_read(&inProc->issues) & pi_SysCallDone)
    inProc->state.regs.fix.EIP += 2;
}

/** @brief Handler function for unbound interrupts. */
void
vh_UnboundIRQ(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
{
  fatal("Received orphaned %s interrupt %d [err=0x%x]\n", 
	inProc ? "process" : "kernel",
	vec->irq, saveArea->Error);
}

/** @brief Handler function for user-mode interrupts. */
void
vh_BoundIRQ(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
{
  assert(!local_interrupts_enabled());

  vec->pending = 1;
  if (!sq_IsEmpty(&vec->stallQ)) {
    vec->next = CUR_CPU->wakeVectors;
    CUR_CPU->wakeVectors = vec;
  }
}

/** @brief Handler function for unbound vectors. */
static void
vh_UnboundVector(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
{
  fatal("Trap from %s to unbound vector %d [err=0x%x]\n", 
      inProc ? "process" : "kernel",
      saveArea->ExceptNo, saveArea->Error);
}

/** @brief Reverse map for finding vectors from IRQ numbers */
VectorInfo *IrqVector[NUM_IRQ];

/** @brief Software-level interrupt dispatch mechanism. */
struct VectorInfo VectorMap[NUM_VECTOR];

// static uint32_t NestedInterrupts[(NUM_TRAP+NUM_IRQ+31)/32];

// const char *hex = "0123456789abcdefg";

// SIMD floating point not handled

void
irq_OnTrapOrInterrupt(Process *inProc, fixregs_t *saveArea)
{
  LOG_EVENT(ety_Trap, inProc, saveArea->ExceptNo, saveArea->Error);

  if (inProc) {
    atomic_write(&CUR_CPU->flags, 0);
    GNU_INLINE_ASM("sti");	/* re-enable interrupts */
  }

  // printf("OnTrapOrInterrupt with vector %d\n", vecno);

  VectorInfo *vector = &VectorMap[saveArea->ExceptNo];

  if (vector->type == vt_Interrupt) { /* hardware interupt */
    irq_t irq = vector->irq;
    IrqController *ctrlr = vector->ctrlr;

    /* The i8259 has ringing problems on IRQ7 and IRQ15.  APICs have
     * race conditions in obscure cases.
     *
     * Rather than wake up user-mode drivers for interrupts that are
     * instantaneously de-asserted, check here whether the interrupt
     * that we allegedly received is still pending. If not, simply
     * acknowledge the PIC and return to whatever we were doing.
     */
    if (! ctrlr->isPending(vector)) {
      fatal("IRQ %d no longer pending\n", irq);
      ctrlr->ack(vector);

      return;
    }

    /* Interrupt actually appears to be pending. This is good. Mask
     * the interrupt at the PIC/IOAPIC and acknowledge to the
     * PIC/LAPIC that we have taken responsibility for it.
     */
    ctrlr->mask(vector);
    ctrlr->ack(vector);

    vector->count++;
    vector->fn(vector, inProc, saveArea);
  }
  else {
    if (inProc)
      mutex_grab(&inProc->hdr.lock);

    vector->count++;
    vector->fn(vector, inProc, saveArea);
  }

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
void vector_init()
{
  for (size_t i = 0; i < NUM_VECTOR; i++) {
    VectorMap[i].type = vt_Unbound;
    VectorMap[i].fn = vh_UnboundVector;
    sq_Init(&VectorMap[i].stallQ);
  }

  for (size_t i = 0; i < 32; i++) {
    VectorMap[i].type = vt_HardTrap;
    VectorMap[i].mode = VEC_MODE_FROMBUS;
    VectorMap[i].level = VEC_LEVEL_FROMBUS;
    VectorMap[i].unmasked = 1;
    VectorMap[i].fn = vh_ReservedException; /* Until otherwise proven. */
  }

  VectorMap[vec_DivZero].fn = vh_UserFault;
  VectorMap[vec_Debug].fn = vh_DebugException;
  VectorMap[vec_NMI].fn = vh_FatalFault;
  VectorMap[vec_BreakPoint].fn = vh_BptTrap;
  VectorMap[vec_Overflow].fn = vh_UserFault;
  VectorMap[vec_Bounds].fn = vh_UserFault;
  VectorMap[vec_BadOpcode].fn = vh_UserFault;
  VectorMap[vec_DeviceNotAvail].fn = vh_DeviceNotAvailable;
  VectorMap[vec_DoubleFault].fn = vh_FatalFault;
  VectorMap[vec_CoprocessorOverrun].fn = vh_UnboundVector;
  VectorMap[vec_InvalTSS].fn = vh_FatalFault;
  VectorMap[vec_SegNotPresent].fn = vh_UserFault;
  VectorMap[vec_StackSeg].fn = vh_UserFault;
  VectorMap[vec_GeneralProtection].fn = vh_UserFault;
  VectorMap[vec_PageFault].fn = vh_PageFault;
  VectorMap[vec_CoprocError].fn = vh_FatalFault;
  VectorMap[vec_AlignCheck].fn = vh_UserFault;
  VectorMap[vec_MachineCheck].fn = vh_FatalFault;
  VectorMap[vec_SIMDfp].fn = vh_UserFault;

  /* Some of the exception vectors have associated instructions that
     can execute from user-mode: */
  VectorMap[vec_BreakPoint].user = 1;
  VectorMap[vec_Overflow].user = 1;
  VectorMap[vec_Bounds].user = 1;

  /* Set up the system call vector. */
  VectorMap[vec_Syscall].type = vt_SysCall;
  VectorMap[vec_Syscall].unmasked = 1;
  VectorMap[vec_Syscall].user = 1;
  VectorMap[vec_Syscall].fn = vh_SysCall;

  assert(sizeof(IdtTable) == sizeof(IdtTable[0])*NUM_VECTOR);

  /* Initialize all hardware table entries. */
  for (uint32_t vec = 0; vec < NUM_VECTOR; vec++) {
    bool isUser = VectorMap[vec].user ? true : false;
    irq_SetHardwareVector(vec, irq_stubs[vec], isUser);
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

VectorInfo *
irq_MapInterrupt(irq_t irq)
{
  switch (IRQ_BUS(irq)) {
  case IBUS_ISA:
    {
      IntSrcOverride isovr;
      if (acpi_map_interrupt(irq, &isovr))
	irq = isovr.globalSystemInterrupt;
      else
	irq = IRQ(IBUS_GLOBAL, IRQ_PIN(irq));
      return IrqVector[irq];
    }
  case IBUS_GLOBAL:
    return IrqVector[irq];

  case IBUS_LAPIC:
      return &VectorMap[IRQ_PIN(irq)];

  default:
    fatal("Unknown bus type for binding\n");
  }
}

/** @brief Set up the software vector table to point to the provided
    handler procedure. */
void
irq_Bind(irq_t irq, uint32_t mode, uint32_t level, VecFn fn)
{
  VectorInfo *vector;

  switch (IRQ_BUS(irq)) {
  case IBUS_ISA:
    {
      mode = VEC_MODE_EDGE;
      level = VEC_LEVEL_ACTHIGH;

      IntSrcOverride isovr;
      if (acpi_map_interrupt(irq, &isovr)) {
	irq = isovr.globalSystemInterrupt;

	if ((isovr.flags & MPS_INTI_TRIGGER) == MPS_INTI_TRIGGER_LEVEL)
	  mode = VEC_MODE_LEVEL;
	if ((isovr.flags & MPS_INTI_POLARITY) == MPS_INTI_POLARITY_ACTIVELOW)
	  mode = VEC_LEVEL_ACTLOW;
      }
      else
	irq = IRQ(IBUS_GLOBAL, IRQ_PIN(irq));

      vector = IrqVector[irq];

      break;
    }
  case IBUS_LAPIC:
    {
      vector = &VectorMap[IRQ_PIN(irq)];
      break;
    }

  default:
    fatal("Unknown bus type for binding\n");
  }

  assert(vector);

  VectorHoldInfo vhi = vector_grab(vector);

  vector->count = 0;
  vector->fn = fn;

  vector->mode = mode;
  vector->level = level;
  vector->ctrlr->setup(vector);

  vector_release(vhi);
}

extern void asm_proc_resume(Process *p) NORETURN;

	/* This is called with interrupts already disabled. */
void 
proc_resume(Process *p)
{
  assert(p == MY_CPU(current));

  ia32_TSS *myTSS = &tss[CUR_CPU->id];
  myTSS->esp0 = (uint32_t) &p->state.regs.fix.ES;

  asm_proc_resume(p);
}


/****************************************************************
 * BRING-UP SUPPORT
 ****************************************************************/
#ifdef BRING_UP
void irq_set_softled(VectorInfo *vi, bool on)
{
  uint32_t vecno = vi - &VectorMap[0];
  if (vecno >= 72)
    return;

#define BlackOnGreen      0x20
#define WhiteOnRed        0x47
#define BlackOnLightGreen 0xa0
#define WhiteOnLightRed   0xc7

  ((char *) 0xc00b8F00)[16 + 2*vecno + 1] = 
    on ? BlackOnLightGreen : WhiteOnLightRed;
}
#endif
