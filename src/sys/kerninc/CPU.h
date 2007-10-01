#ifndef __KERNINC_CPU_H__
#define __KERNINC_CPU_H__
/*
 * Copyright (C) 2007, The EROS Group, LLC.
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
 * @brief Machine-independent per-CPU information.
 */

#include <stddef.h>
#include <stdbool.h>
#include <hal/config.h>
#include <hal/atomic.h>
#include <hal/kerntypes.h>
#include <kerninc/ccs.h>

#if MAX_NCPU <= 256
typedef uint8_t cpuid_t;
#else
typedef uint32_t cpuid_t;
#error "Need to reconsider type of MutexValue fields"
#endif

/** @brief Process executing on current CPU has been preempted. */
#define CPUFL_WAS_PREEMPTED 0x1
/** @brief Wakeup processing is required on current CPU.
 *
 * This bit is "owned" by the interval management logic. It is set
 * and cleared in the interval code, and it is checked just before
 * returning from traps/interrupts to determine if wakeup processing
 * is needed. 
 */
#define CPUFL_NEED_WAKEUP  0x2

typedef struct CPU {
  /** @brief Mutex value for locks held by current process on this
   * CPU.
   *
   * This one is gang released. It is stored here so that spinlock can
   * check for abandoned locks.
   */
  uint32_t procMutexValue;

  /** @brief Unique identifier for this CPU. */
  cpuid_t   id;

  /** @brief Starting (least) address of per-CPU stack. */
  kva_t     stack;

  /** @brief TRUE means this CPU should yield whenever mutex_trylock()
   * does not immediately succeed and we are NOT in a
   * mutex_spinlock().
   */
  bool       shouldDefer;

  /** @brief true iff this CPU is present. */
  bool       present;

  /** @brief true iff this CPU has been started. */
  bool       active;

  /** @brief Priority of current process on CPU */
  uint32_t   priority;

  /** @brief Per-CPU action flags for this CPU.
   * 
   * Zero on kernel entry. Bits set in various places if the
   * preemption timer goes off, and/or if we need to do wakeup
   * processing on the sleeping process queue.
   */
  Atomic32_t  flags;

  /** @brief Mapping context currently loaded on this CPU.
   *
   * Corner case: if the CPU has not yet been IPL'd, this is the map
   * that @em will be loaded onto that CPU during IPL. We need to mark
   * such a map loaded so that the map will not be reclaimed. */
  struct Mapping *curMap;

  /** @brief Allocated object recovery pointer.
   *
   * When an object is allocated, there is a (hopefully brief) window
   * of time when the object is not on any list. Ideally, we should
   * not yield during this period, but on some architectures that
   * constraint is inconvenient and it is not always obvious to the
   * programmer which functions may yield. There is only one such
   * object outstanding at a time, so our recovery strategy is to keep
   * track of the pointer here along with a callback that knows how to
   * re-free the object (back on to the appropriate free list).
   */
  void *orphanedObject;
  /** @brief Procedure that knows how to put the orphanedObject onto
   * its appropriate free list.
   */
  void (*yieldAllocFixup)(void *);
} CPU;

/* Guaranteed <= MAX_NCPU, defined in hal/machine.h */
extern size_t cpu_ncpu;

/** @brief Vector of all possible CPU structures */
extern CPU cpu_vec[MAX_NCPU];

/** @brief Initialize the machine-independent CPU structure for a
 * given CPU.
 */
void cpu_construct(cpuid_t ndx);

/** @brief Return the CPUID (index into cpu vector) of the currently
 * executing processor.
 *
 * This is NOT the preferred way to do this, as it is fairly
 * expensive.
 */
__hal cpuid_t cpu_getMyID();

/** @brief Per-CPU pointer to the current CPU. */
DECLARE_CPU_PRIVATE(CPU*,curCPU);

#endif /* __KERNINC_CPU_H__ */
