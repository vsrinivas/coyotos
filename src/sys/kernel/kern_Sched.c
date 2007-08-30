/*
 * Copyright (C) 2006, The EROS Group, LLC.
 *
 * This file is part of the EROS Operating System.
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
 * @brief Scheduling logic.
 */

#include <kerninc/ccs.h>
#include <kerninc/printf.h>
#include <kerninc/Process.h>
#include <kerninc/ReadyQueue.h>
#include <kerninc/Sched.h>
#include <kerninc/assert.h>
#include <kerninc/event.h>
#include <hal/machine.h>
#include <hal/irq.h>

void
sched_abandon_transaction()
{
  Process *p = MY_CPU(current);

  LOG_EVENT(ety_AbandonTX, p, 0, 0);

  /* Release all locks held by current process. */
  mutex_release_all_process_locks();

  /* Once current process is off CPU, it's mapping structures may be
   * deallocated, so ensure that we aren't running off of them: */
  vm_switch_curcpu_to_map(vm_percpu_kernel_map());

  if (p) {
    /* Current process no longer on CPU. */
    p->onCPU = NULL;

    /* Since process is being abandoned, take notice that we have
       already performed a preemption in case the interval timer goes off. */
    MY_CPU(curCPU)->hasPreempted = true;
    p->issues &= ~pi_Preempted;

    /// @bug Should this path put the process back on to the ready queue
    /// if it is still running? This case can arise if we are preempted
    /// late. I think the answer is probably NO. In fact, we should
    /// probably be validating here that the process is on a stall
    /// queue.
    ///
    /// @bug The following assert is wrong. A process in the "faulted"
    /// state should not be on a queue. Only a process in the "running"
    /// state should be on a queue.

#ifndef NDEBUG
    {
      uint32_t pstate = p->state.runState;

      /* A process that abandons a transaction while running ought to be
       * on a stall queue.
       *
       * Further, it should NOT be on the ready queue.  XXX I think
       * this is false; proc_dispatch_current() places the process on
       * the runqueue and abandons the transaction.
       */
      if (pstate == PRS_RUNNING)
	assert(p->onQ != 0);

      /* A process that is faulted or receiving should not be on a
	 stall queue. */
      if ((pstate == PRS_FAULTED) || (pstate == PRS_RECEIVING))
	assert(p->onQ == 0);
    }
#endif
  }

  /* It's official. Process is no longer running on this CPU. */
  MY_CPU(current) = 0;

  /// Call the HAL layer to find something useful to do.
  sched_low_level_yield();
}

void
sched_restart_transaction()
{
  LOG_EVENT(ety_RestartTX, MY_CPU(current), 0, 0);

  // FIX: Consider NOT releasing held process locks. Might we still
  // want them?
  mutex_release_all_process_locks();

  /* If process is restarting transaction, it should still be running,
     and it should NOT be on a stall queue. */
  assert(MY_CPU(current)->onQ == 0 && MY_CPU(current)->onCPU != 0);

  sched_low_level_yield();
}

Process *
sched_choose_next()
{
  if (MY_CPU(current))
    return MY_CPU(current);
  Process *p = rq_removeFront(&mainRQ);
  if (p) {
    mutex_grab(&p->hdr.lock);
    p->onCPU = MY_CPU(curCPU);
  }

  return p;
}

void
sched_dispatch_something()
{
  // This is moderately subtle. When we call proc_dispatch_current(),
  // exactly one of the following things will occur:
  //
  //  1. The process will be successfully dispatched, or
  //  2. The process will end up on a stall queue, after which
  //     sched_resched() will be called.
  //
  // In either case, the call to proc_dispatch_current() will not
  // return.
  //
  // If sched_resched() is called, we will abandon the stack and
  // re-enter the scheduler from above. Therefore, if the attempt to
  // dispatch fails we will try again with the next candidate until we
  // manage to dispatch something.

  for ( ;; ) {
    assert(local_interrupts_enabled());
    MY_CPU(current) = sched_choose_next();
    if (MY_CPU(current))
      proc_dispatch_current();
    else {
      printf("Idling current CPU\n");
      IdleThisProcessor();
    }
  }

  fatal("Dispatch restart failed.\n");
}
