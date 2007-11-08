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
#include <hal/cpu.h>
#include <kerninc/ccs.h>

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

struct Process;

typedef struct CPU {
  /** @brief Mutex value for locks held by current process on this
   * CPU.
   *
   * This one is gang released. It is stored here so that spinlock can
   * check for abandoned locks.
   */
  uint32_t procMutexValue;

  /** @brief Process that is presently running on this CPU (if any). */
  struct Process *current;

  /** @brief Unique identifier for this CPU. */
  cpuid_t   id;

  /** @brief Bitmap of this CPU's available transmap entries. */
  transmeta_t  TransMetaMap;

  /** @brief Bitmap of this CPU's available transmap entries that have
   * been released but not yet flushed from the TLB. */
  transmeta_t  TransReleased;

  /** @brief Per-CPU kernel stack reload address. */
  kva_t     topOfStack;

  /** @brief If shouldDefer matches procMutexValue, this CPU has been
   * asked to get out of the way if it cannot aquire a mutex immediately.
   */
  Atomic32_t shouldDefer;

  /** @brief true iff this CPU is present. */
  bool       present;

  /** @brief true iff this CPU has been started. */
  bool       active;

  /** @brief Priority of current process on CPU */
  Atomic32_t   priority;

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

  /** @brief Linked list of vector entries that we need to awaken.
   *
   * This needs to be manipulated with interrupts disabled.
   */
  struct VectorInfo *wakeVectors;
} CPU;

/* Guaranteed <= MAX_NCPU, defined in hal/machine.h */
extern size_t cpu_ncpu;

/** @brief Vector of all possible CPU structures */
extern CPU cpu_vec[MAX_NCPU];

/** @brief Initialize the machine-independent CPU structure for a
 * given CPU.
 */
void cpu_construct(cpuid_t ndx);

/** @brief Wake up the processes that are blocked waiting for pending
 * interrupts on this CPU.
 */
void cpu_wake_vectors();

#if MAX_NCPU > 1
#define CUR_CPU (current_cpu())
#else
#define CUR_CPU (&cpu_vec[0])
#endif

#define MY_CPU(id) CUR_CPU->id

/** @brief Return the CPU structure pointer (entry in cpu vector) of
 * the currently executing processor.
 *
 * This is NOT the preferred way to do this, as it is fairly
 * expensive. The difference between this and current_cpu is that it
 * can be called by a processor whose stack is not yet set up.
 */
struct CPU* cpu_getMyCPU();

/** @brief IPL any attached processors */
__hal extern void cpu_init_all_aps(void);
__hal extern void cpu_start_all_aps(void);

#endif /* __KERNINC_CPU_H__ */
