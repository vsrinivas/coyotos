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

#define NUM_VECTOR   (NUM_TRAP+NUM_IRQ)

/** @brief How we use the interrupt vectors. */
enum Vectors {
  vec_DivZero 		 = 0x0,
  vec_Debug 		 = 0x1,
  vec_NMI 		 = 0x2,
  vec_BreakPoint 	 = 0x3,
  vec_Overflow 		 = 0x4,
  vec_Bounds 		 = 0x5,
  vec_BadOpcode 	 = 0x6,
  vec_DeviceNotAvail	 = 0x7,
  vec_DoubleFault 	 = 0x8,
  vec_CoprocessorOverrun = 0x9,
  vec_InvalTSS 		 = 0xa,
  vec_SegNotPresent 	 = 0xb,
  vec_StackSeg 		 = 0xc,
  vec_GeneralProtection  = 0xd,
  vec_PageFault 	 = 0xe,
  vec_CoprocError	 = 0x10,
  vec_AlignCheck         = 0x11,
  vec_MachineCheck       = 0x12,
  vec_SIMDfp             = 0x13,

  /* Vectors in the range [0x14, 0x1f] are reserved. */

  vec_IRQ0 		 = NUM_TRAP,

  /* Following fall within the "interrupt space" portion of the
     numbering, but must not collide with hardware interrupts. */
  vec_Syscall		 = 0x30,
  vec_LegacySyscall      = 0x80,
};

enum LegacyInterrupts {
  irq_PIT       = 0,		/* CMOS Interval Timer */
  irq_Keyboard  = 1,		/* Keyboard */
  irq_Cascade   = 2,	      /* master 8259 cascade from secondary */
};

/** @brief Initialize the vector table and the corresponding IDT
 * entries.
 */
void irq_vector_init(void);

/** @brief Enable interrupt handling on current CPU.
 *
 * When called for the first time, also initializes the interrupt
 * handling tables.
 */
void irq_init(void);

typedef void (*VecFn)(Process *, fixregs_t *saveArea);

/** @brief Vector types.
 *
 * Values for VectorInfo.type field.
 */
enum VecType {
  vt_Unbound,		/**< @brief Not yet bound.  */
  vt_HardTrap,		/**< @brief Hardware trap or exception.  */
  vt_SysCall,		/**< @brief System call (software trap).  */
  vt_Interrupt,		/**< @brief Interrupt source.  */
};
typedef enum VecType VecType;

/** @brief Per-vector information.
 *
 * This structure stores the machine-dependent mapping from vectors to
 * handlers, and also from vectors to interrupt lines.
 *
 * The stall queue associated with a given interrupt pin is actually
 * stored in the VectorInfo structure.
 */
struct VectorInfo {
  VecFn    fn;			/**< @brief Handler function. */
  uint64_t count;		/**< @brief Number of occurrences. */
  uint8_t  type;		/**< @brief See VecType. */
  uint8_t  user : 1;		/**< @brief User accessable */
  uint8_t  edge : 1;		/**< @brief Edge triggered */
  uint8_t  enabled : 1;		/**< @brief Vector enabled  */
  uint8_t  irqSource;		/**< @brief Interrupt pin number. */
  // StallQ stallQ;
};
typedef struct VectorInfo VectorInfo;
extern struct VectorInfo VectorMap[NUM_VECTOR];
extern uint8_t IrqVector[NUM_IRQ];

#define IRQ_NO_VECTOR 0
#define VECTOR_NO_IRQ 255

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
void irq_EnableInterrupt(uint32_t irq);
void irq_DisableInterrupt(uint32_t irq);

void irq_BindInterrupt(uint32_t irq, VecFn fn);
void irq_UnbindInterrupt(uint32_t irq);

void irq_DoTripleFault() NORETURN;

#endif /* __I686_IRQ_H__ */
