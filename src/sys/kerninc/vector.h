#ifndef __KERNINC_VECTOR_H__
#define __KERNINC_VECTOR_H__
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
 * @brief Common interrupt distribution logic.
 *
 * While the low-level handling of interrupts is architecture
 * dependent, the high-level handling is more or less common, and the
 * information that needs to be tracked concerning interrupt vectors
 * is likewise more or less common.
 *
 * Coyotos uses a single vector to describe exceptions, traps, system
 * calls, and interrupts. A given target MUST use this vector for
 * interrupts, but it is free to handle traps and system calls through
 * other means if that is the preferred implementation on the
 * target. Typically, the VectorMap array indices will correspond to
 * hardware vector numbers that are either supplied by the hardware
 * directly or injected by the low-level trap interface.
 *
 * When Coyotos code refers to an IRQ, it is referring to a
 * sequentially numbered hardware pin number. When it refers to a
 * vector number, it is referring to a hardware-supplied index
 * number. A subset of entries in the VectorMap array will describe
 * IRQs. Because system calls may be interspersed with irq entries, it
 * is necessary to maintain both forward and backward mappings between
 * vector numbers and IRQ numbers "by hand".
 * 
 * Concrete examples:
 *
 * - IA-32 handles traps, interrupts, and SYSCALLS through the same
 *   hardware vector mechanism, and system calls need to be
 *   interspersed with interrupts in the hardware
 *   vector. On this platform, NUM_VECTOR=NUM_TRAP+NUM_IRQ
 * - Coldfire has a hardware-supplied system call mechanism that uses
 *   a separate entry point, and (from memory) much of its interrupt
 *   demultiplexing is done in software. On that platform We wil
 *   probably use NUM_VECTOR=NUM_IRQ, and demultiplex traps and system
 *   calls using machine-dependent structures and code.
 */

#include <hal/irq.h>
#include <kerninc/Process.h>
#include <kerninc/StallQueue.h>

struct VectorInfo;

/**@brief Signature of a vector handler function. */
typedef void (*VecFn)(struct VectorInfo *vec, struct Process *,
		      fixregs_t *saveArea);

/** @brief Method dispatch for interrupt controller chips */
struct IrqController {
  irq_t baseIRQ;	   /**< @brief First IRQ on this controller */
  irq_t nIRQ;		   /**< @brief Number of IRQ sources */
  kva_t va;			/**< @brief For memory-mapped controllers. */
  void (*setup)(struct VectorInfo *vi);
  bool (*isPending)(struct VectorInfo *vi);
  void (*unmask)(struct VectorInfo *vi);
  void (*mask)(struct VectorInfo *vi);
  void (*ack)(struct VectorInfo *vi);
};
typedef struct IrqController IrqController;

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

#define VEC_MODE_FROMBUS 0
#define VEC_MODE_EDGE    1
#define VEC_MODE_LEVEL   2

#define VEC_LEVEL_FROMBUS  0
#define VEC_LEVEL_ACTHIGH  1
#define VEC_LEVEL_ACTLOW   2

/** @brief Per-vector information.
 *
 * This structure stores the machine-dependent mapping from vectors to
 * handlers, and also from vectors to interrupt lines. Note that
 * interrupts 
 *
 * The stall queue associated with a given interrupt pin is actually
 * stored in the VectorInfo structure.
 */
struct VectorInfo {
  VecFn    fn;			/**< @brief Handler function. */
  uint64_t count;		/**< @brief Number of occurrences. */
  uint8_t  type;		/**< @brief See VecType. */
  uint8_t  user : 1;		/**< @brief User accessable */
  uint8_t  mode : 2;		/**< @brief Trigger mode */
  uint8_t  level : 2;		/**< @brief Active hi/lo */
  uint8_t  unmasked : 1;	/**< @brief Vector unmasked at ctrlr chip  */
  uint8_t  pending : 1;		/**< @brief Interrupt accepted on this
				   vector.  */
  uint32_t disableCount;	/**< @brief Number of application
				   disable requests for this vector.  */
  uint32_t  irq;		/**< @brief Global interrupt pin number. */
  IrqController* ctrlr;		/**< @brief Controller chip */
  StallQueue stallQ;

  struct VectorInfo *next;
};
typedef struct VectorInfo VectorInfo;

/** @brief per-vector information. */
extern struct VectorInfo VectorMap[NUM_VECTOR];

/** @brief Reverse map from IRQs to vector entries. */
extern VectorInfo *IrqVector[NUM_IRQ];

/** @brief Initialize the vector table and the corresponding IDT
 * entries.
 */
__hal void vector_init(void);

typedef struct VectorHoldInfo {
  SpinHoldInfo shi;
  flags_t oldFlags;
} VectorHoldInfo;

/** @brief Lock a vector entry for manipulation, including guarding
 * against interrupts.
 */
static inline VectorHoldInfo vector_grab(VectorInfo *v)
{
  VectorHoldInfo vhi;
  vhi.oldFlags = locally_disable_interrupts();
  vhi.shi = spinlock_grab(&v->stallQ.qLock);

  return vhi;
}

/** @brief Release a vector entry, allowing further interrupts or
 * activity on this vector.
 */
static inline void vector_release(VectorHoldInfo vhi)
{
  spinlock_release(vhi.shi);
  locally_enable_interrupts(vhi.oldFlags);
}

/** @brief Enable specified interupt pin. */
void irq_EnableVector(irq_t irq);
/** @brief Disable specified interupt pin. */
void irq_DisableVector(irq_t irq);

/** @brief Return true IFF vector is currently enabled. */
bool irq_isEnabled(irq_t irq);

/** @brief Bind an interrupt to a device.
 *
 * Note that this is generally @em not used to bind the interrupt to a
 * particular end device. There are one or two devices in the kernel
 * for which that is true, but the more usual case is that this gets
 * invoked indirectly from user-land bus device drivers to bind the
 * interrupt source to the bus controller. Further demultiplexing to
 * the actual device will happen in user-mode code.
 *
 * In consequence, there is currently no provision in the kernel for
 * @em undoing this operation. */
void irq_Bind(irq_t irq, uint32_t mode, uint32_t level, VecFn fn);

/** @brief Given an IRQ, return the appropriate vector entry. */
VectorInfo *irq_MapInterrupt(irq_t irq);

/** @brief Handler to use prior to interrupt binding, as a sanity
 * provision. */
__hal void vh_UnboundIRQ(VectorInfo *vec, Process *inProc, 
			 fixregs_t *saveArea);

#endif /* __KERNINC_VECTOR_H__ */
