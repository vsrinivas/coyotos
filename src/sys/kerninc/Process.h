#ifndef __KERNINC_PROCESS_H__
#define __KERNINC_PROCESS_H__
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
 * @brief Definition of process structure. */

#include <stddef.h>
#include <hal/vm.h>
#include <kerninc/ccs.h>
#include <kerninc/capability.h>
#include <kerninc/Link.h>
#include <kerninc/StallQueue.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/CPU.h>
#include <kerninc/Sched.h>
#include <kerninc/Interval.h>
#include <obstore/Process.h>
#include <idl/coyotos/Process.h>

/**
 * @brief Reasons that a process cannot run.
 *
 * Later issues have to do with required functional units. If you run
 * in to an architecture with a new type of functional unit, add an
 * appropriate entry, but try to use a suitably architecture-neutral
 * name. For example, on IA-32 the pi_VectorUnit refers to SSE2/SSE3
 * state.
 */
enum Issues {
  pi_Schedule     = 0x0001u,  /**< @brief Scheduling criteria are unknown. */
  pi_Faulted      = 0x0002u,  /**< @brief Process has non-zero fault code. */
  pi_SingleStep   = 0x0004u,  /**< @brief A single step has been requested. */
  pi_Preempted    = 0x0008u,  /**< @brief Process has been preempted */

  pi_NumericsUnit = 0x1000u,  /**< @brief Process needs the FPU */
  pi_VectorUnit   = 0x2000u,  /**< @brief Process needs the vector unit */
  pi_SysCallDone  = 0x4000u,  /**< @brief System call has completed.
			       *
			       * This "issue" indicates that the CPU
			       * is free to return using the system
			       * call return protocol, as opposed to
			       * the trap return protocol.
			       *
			       * Note the design assumes that
			       * returning via the trap return
			       * protocol is @em always valid,
			       * regardless of entry method. This is
			       * necessary because system calls may
			       * abort and need to be restarted, and
			       * on many hardware implementations said
			       * restart cannot be implemented via the
			       * hardware system call return mechanism.
			       */

};
typedef enum Issues Issues;

/** @brief Issues that should be set when a process is initially
    loaded. */
#define  pi_IssuesOnLoad (pi_Schedule)

/** @brief The process structure.
 *
 * Every running activity has an associated process structure. This
 * contains the register state of the process. In contrast to EROS,
 * there are no in-kernel processes in Coyotos.
 * 
 * Processes are reallocated in round-robin order, which may well prove
 * to be a dumb thing to do.  Kernel processes are not reallocated.
 *
 *
 * The substates of PRS_RUNNING are spelled out here:
 *
 *   wiki:KernelInternals/ProcessLocking
 *
 * @section Locking
 *
 * @p queue_link and @p queue are protected by the lock of the queue
 * the process is on.  Note that a process can change queues, so care
 * must be taken in acquiring the queue lock while holding the process lock.
 *
 * As long as the process's @p onCPU field is NULL, all other fields are
 * protected by the process's header's lock. 
 *
 * If @p onCPU is non-NULL, some of the state of the process is loaded
 * into a CPU.  The following restrictions must be followed:
 *
 * @li 
 *   @p fixregs, @p floatregs, @p softregs, @p faultCode, and @p
 *   faultInfo are completely undefined, and cannot be read or written.
 * @li
 *   All capabilities in the Process state can only be read.
 * @li
 *   The runState cannot be changed.
 *   
 * To clear @p onCPU when it is not on our CPU, a cross-call to the
 * CPU in question is required, with the Process's header's lock held
 * across the crosscall.  After onCPU is cleared, the process must either
 * change runState or be placed onto a ready queue before the header lock
 * is dropped for any reason.
 */

struct Process {
  struct ObjectHeader hdr;

  /** @brief Reasons that the process cannot run.
   *
   * The issues field is a bitmask that identifies any conditions that
   * preclude this process from making progress if dispatched. If the
   * issues field is zero, the process has no issues and is immediately
   * dispatchable. If the field is non-zero, some form of special
   * handling is required before dispatch is possible.
   *
   * Meanings of individual bit positions are defined above in the
   * Issues enumeration. 
   */
  Atomic32_t        issues;

  cpuid_t           lastCPU;

  /** @brief Ready queue to go on when we wake up.
   *
   * A process can have different scheduling clases. Depending on the
   * scheduling class, it will go on different ready queues when it is
   * ready to run. We cache a pointer to the appropriate ready queue
   * in the process structure.
   */
  struct ReadyQueue *readyQ;

  /** @brief the stall (or ready) queue we are presently on.
   *
   * If NULL, setting this field is protected by the Process's header's lock.
   * If non-NULL, setting or clearing this field is protected by onQ->qLock.
   */
  struct StallQueue *onQ;

  /** @brief the CPU which the process is currently running on.
   *
   * May only be set to a non-NULL value with hdr.lock held.  After it is
   * set, may only be cleared by the CPU it points to.
   */
  struct CPU       *onCPU;

  /** @brief For a process that is sleeping: when to wake up. */
  Interval          wakeTime;

  /** @brief Link structure used when this process is on a stall queue
   * or ready queue. 
   *
   * The contents of this field are protected by onQ->qLock, if onQ is
   * non-NULL.
   */
  Link		    queue_link;

  struct Mapping    *mappingTableHdr;

  /** @brief Stall queue for all processes waiting to send to this
   * one. */
  StallQueue        rcvWaitQ;

  /** @brief Non-zero when we are in an extended IPC transaction with
   * a peer.
   *
   * Invariant: ((ipcPeer == 0) || (p->ipcPeer->ipcPeer == p) */
  struct Process *  ipcPeer;

  ExProcess         state;
};
typedef struct Process Process;

static inline Link *
process_to_link(Process *p)
{
  if (!p)
    return 0;
  return &p->queue_link;
}

static inline Process *
process_from_link(Link *l)
{
  if (!l)
    return 0;
  return (Process *)((uintptr_t)l - offsetof(struct Process, queue_link));
}

DECLARE_CPU_PRIVATE(Process*,current);

//#ifdef COYOTOS_HAVE_FPU
///* FPU support: */
//DECLARE_PER_CPU(extern Process*,proc_fpuOwner);
//#endif

extern Process *proc_ProcessCache;

static inline void
proc_SetFault(Process *p, uint32_t code, uva_t faultInfo)
{
  p->state.faultCode = code;
  p->state.faultInfo = faultInfo;
  atomic_set_bits(&p->issues, pi_Faulted);
}

static inline void
proc_TakeFault(Process *p, uint32_t code, uva_t faultInfo) NORETURN;

static inline void
proc_TakeFault(Process *p, uint32_t code, uva_t faultInfo)
{
  proc_SetFault(p, code, faultInfo);
  sched_restart_transaction();
}

extern void proc_gc(Process *);

/** @brief Attempt to dispatch the passed process. 
 *
 * If the process constituents are not in memory, starts the work to
 * bring them in and abandon the transaction. If the process has
 * faulted, attempts to deliver the fault message to the keeper, else
 * enqueues faulted process on keeper and returns. Abandons current
 * transaction if any of this causes specified process to block.
 */
void proc_dispatch_current() NORETURN;

/** @brief Clear any architecture-dependent issues that would
 * preclude dispatching this process.
 *
 * Returns only if successful.
 */
void proc_clear_arch_issues(Process *p);

/** @brief Find a per-process mapping table that is appropriate for
 * executing on the current CPU.
 */
void proc_migrate_to_current_cpu();

/** @brief Initialize process registers to a reasonable state.
 */
void proc_arch_initregs(uva_t pc);

/** @brief Set process address space to cpu-private mapping.
 */
void proc_load_cpu_private_map();

/** @brief Called from HAL when current process makes a system call.
 */
__hal void proc_syscall(void);

/** @brief Deliver a memory fault that accrues to process @p.
 *
 * There are three cases where this routine is called:
 *
 * -# During a normal page fault. In this case @p p matches
 *    @c current.
 * -# During input argument processing in invocations. This is
 *    logically just like the page fault case. @p p matches 
 *    @c current. 
 * -# During @em output argument processing during invocations. This
 *    is a cross-space case. @p p does @em not equal @c current.
 *
 * In all cases, @p p will eventually end up in the PRS_FAULTED
 * state, but the mechanism varies. See comments in the implementation
 * for details.
 */
struct MemWalkResults;

void proc_deliver_memory_fault(Process *p,
			       coyotos_Process_FC fc, uintptr_t addr, 
			       const struct MemWalkResults * const mwr);

/** @brief Ensure that it is safe to manipulate state of an already-locked
 * process.
 *
 * On a single-CPU system, this is a no-op.  On multi-CPU systems, it is
 * possible that a process is on-CPU even though we are holding its lock.
 *
 * If run-state is not PRS_RUNNING, it is not necessary to call this routine.
 *
 * If run-state *is* PRS_RUNNING, is not safe to:
 *   <ol><li>modify anything in the "state" member, or</li>
 *       <li>read any non-capability field of "state", except for runState</li>
 *   </ol>
 * without first calling this routine.
 */
void proc_ensure_exclusive(Process *p);

/** @brief Results of <tt>proc_findCapPageSlot</tt> and/or
 *  <tt>proc_findDataPage</tt>, naming a slot in a  
 *  CapPage and the restrictions on its use.
 */
typedef struct FoundPage {
  struct Page *pgHdr;	/**< @brief The CapPage targeted */
  size_t slot;		/**< @brief The slot number to use */
  uint8_t restr;	/**< @brief The restrictions on use of the slot */
} FoundPage;

/** @brief Locate the CapPage slot in address space of process @p p at
 * address @p addr.
 *
 * Returns a non-zero fault code on error, without touching @p results.
 * On success, returns zero, and fills @p results with information necessary
 * to use the slot.
 *
 * Precondition: Process @p p's lock must be held.
 *
 * Postcondition: 
 *    On zero return, results->capPage is locked, and 
 *                    results->restr is the cumulative restrictions of the 
 *                    path to results->capPage.
 */
extern coyotos_Process_FC
proc_findCapPageSlot(Process *p, uintptr_t addr, bool forWriting,
		     bool nonBlock,
		     struct FoundPage * /*OUT*/ results);

/** @brief Locate the Page in Process @p p's address space at
 * address @p addr.
 *
 * Returns a non-zero fault code on error, without touching @p results.
 * On success, returns zero, and fills @p results with information necessary
 * to use the slot.
 *
 * Precondition: Process @p p's lock must be held.
 *
 * Postcondition: 
 *    On zero return, results->pgHdr is locked, and 
 *                    results->restr is the cumulative restrictions of the 
 *                    path to results->capPage.
 */
coyotos_Process_FC
proc_findDataPage(Process *p, uintptr_t addr, bool forWriting,
		  bool nonBlock,
		  struct FoundPage * /*OUT*/ results);

/** @brief Resume execution of the current process.
 *
 * Returns to user land executing the current process. This is a
 * low-level assembly language entry point supplied by the
 * architecture-dependent code.
 *
 * This should always be called with local interrupts DISABLED.
 */
__hal void proc_resume(Process *p) NORETURN;

/* Debugging only */
__hal void proc_dump_current_savearea();
#endif /* __KERNINC_PROCESS_H__ */
