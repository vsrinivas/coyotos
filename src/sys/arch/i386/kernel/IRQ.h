#ifndef __I686_IRQ_H__
#define __I686_IRQ_H__
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
 * @brief IDT-related function declarations.
 */

#include <stdbool.h>
#include <kerninc/ccs.h>
#include <kerninc/Process.h>
#include <hal/irq.h>

/** @brief How we use the interrupt vectors. */
enum IrqVectors {
  iv_DivZero 		= 0x0,
  iv_Debug 		= 0x1,
  iv_NMI 		= 0x2,
  iv_BreakPoint 	= 0x3,
  iv_Overflow 		= 0x4,
  iv_Bounds 		= 0x5,
  iv_BadOpcode 		= 0x6,
  iv_DeviceNotAvail	= 0x7,
  iv_DoubleFault 	= 0x8,
  iv_CoprocessorOverrun = 0x9,
  iv_InvalTSS 		= 0xa,
  iv_SegNotPresent 	= 0xb,
  iv_StackSeg 		= 0xc,
  iv_GeneralProtection 	= 0xd,
  iv_PageFault 		= 0xe,
  iv_CoprocError	= 0x10,
  iv_AlignCheck         = 0x11,
  iv_MachineCheck       = 0x12,
  iv_SIMDfp             = 0x13,

  /* Vectors in the range [0x14, 0x1f] are reserved. */

  iv_IRQ0 		= NUM_TRAP,
  iv_Legacy_PIT         = NUM_TRAP,    /* IRQ0. CMOS interval timer */
  iv_Legacy_Keyboard    = NUM_TRAP+1,  /* IRQ1. Keyboard  */
  iv_8259_Cascade       = NUM_TRAP+1,  /* IRQ2. Should never happen. */

  /* Following fall within the "interrupt space" portion of the
     numbering, but must not collide with hardware interrupts. */
  iv_Syscall		= 0x30,
  iv_LegacySyscall      = 0x80,
};

/** @brief Initialize the interrupt descriptor table.
 */
void irq_vector_init(void);

/** @brief Initialize the interrupt vector pointer hardware on the
 * current CPU.
 */
void irq_init(void);

typedef void (*IrqVecFn)(Process *, fixregs_t *saveArea);

enum IntVecFlags {
  /** @brief 1 iff a handler is registered for this interrupt. */
  ivf_bound = 0x1u,
  /** @brief 1 if this interrupt is logically enabled. */
  ivf_enabled = 0x2u,
  /** @brief 1 if this interrupt is a hardward trap (system call). */
  ivf_hardTrap = 0x4u,
  /** @brief 1 iff this interrupt is currently configured in legacy
      PIC mode */
  ivf_legacyPIC = 0x4u,
};

/** @brief Per-vector interrupt information.
 *
 * @todo This should probably be machine-independent.
 */
struct IntVecInfo {
  IrqVecFn fn;			/**< @brief Handler function */
  uint64_t count;		/**< @brief Number of occurrences. */
  uint32_t flags;		/**< @brief See IntVecFlags. */
  // StallQ stallQ;
};
typedef struct IntVecInfo IntVecInfo;

/** @brief Dispatcher for an interrupt or exception that diverted us
 * from userland.
 *
 * Note that this function will not always return, because in many cases we
 * will have yielded, and in that case we will be returning to another
 * process. The function will return if:
 *
 * - This was a kernel interrupt.
 * - This was a user interrupt that did NOT cause a preemption, yield,
 *   or context switch. Check the interrupt.S code that follows
 *   the user-mode call to OnTrapOrInterrupt in this case, because it
 *   is doing a fairly sleazy flow-through.
 *
 */
void irq_OnTrapOrInterrupt(Process *inProc, fixregs_t *saveArea);
void irq_EnableVector(uint32_t vector);
void irq_DisableVector(uint32_t vector);

void irq_BindVector(uint32_t vector, IrqVecFn fn);
void irq_UnbindInterrupt(uint32_t vector);

void irq_DoTripleFault() NORETURN;

#endif /* __I686_IRQ_H__ */
