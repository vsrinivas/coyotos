/*
 * Copyright (C) 2007, The EROS Group, LLC.
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
 * @brief Initial interval timer configuration
 *
 * Coyotos requires that some form of initial timer be set up to drive
 * round-robin scheduling during early application-level
 * bootstrap. The interval timer will rapidly be taken over by
 * user-mode driver code, but an initial setting is helpful in order
 * drive an initial round-robin policy while the drivers sort
 * themselves out during boot.
 *
 * If we have a local APIC available, we want to use the interval
 * timer on the local APIC. Otherwise, use the square-wave generator
 * on the CMOS chip.
 */

#include "IRQ.h"
#include "PIC.h"
#include <kerninc/printf.h>
#include <kerninc/assert.h>
#include <kerninc/Sched.h>
#include <kerninc/ReadyQueue.h>
#include <kerninc/event.h>
#include <coyotos/i386/io.h>

// #define CMOS_TICK_DIVIDER	11931 /* (0x2e9b) ~10ms */
#define CMOS_TICK_DIVIDER	23863 /* (0x2e9b) ~50/sec */
#define CMOS_TIMER_PORT_0	0x40
#define CMOS_TIMER_MODE		0x43
#define CMOS_SQUARE_WAVE0	0x34

static void 
pit_wakeup(Process *inProc, fixregs_t *saveArea)
{
  uint32_t vecno = saveArea->ExceptNo;

  //  printf("%s Timer Interrupt!\n", inProc ? "Process" : "Kernel");

  /* Re-enable the periodic timer interrupt line: */
  pic_enable(vecno);

  /* Preemption has occurred. */
  if (inProc) {
    LOG_EVENT(ety_UserPreempt, inProc, 0, 0);
    MY_CPU(curCPU)->hasPreempted = true;
    rq_add(&mainRQ, inProc, 0);
    sched_abandon_transaction();
  }

  /* Interrupt hit us in the kernel. Arrange for a later transaction
   * abort if a preemption has not already occurred.
   *
   * If the timer was configured as free-running, it may be set in
   * motion before there is any process on the current CPU. In that
   * case there is nothing to preempt yet.
     */
  if (!MY_CPU(curCPU)->hasPreempted && MY_CPU(current)) {
    /* If the timer is free-running, it may be set in motion before
     * there is any process on the current CPU. In that case there is
     * nothing to preempt yet.
     */
    MY_CPU(current)->issues |= pi_Preempted;
    MY_CPU(curCPU)->hasPreempted = true;

    LOG_EVENT(ety_KernPreempt, MY_CPU(current), 0, 0);
    printf("Current process preempted by timer (from %s).\n",
	   inProc ? "user" : "kernel");
  }

  return;
}

void
hardclock_init()
{
  if (pic_have_lapic()) {
    bug("Need to configure PIT for round-robin\n");
  }
  else {

    outb(CMOS_SQUARE_WAVE0, CMOS_TIMER_MODE);
    outb(CMOS_TICK_DIVIDER & 0xffu, CMOS_TIMER_PORT_0);
    outb(CMOS_TICK_DIVIDER >> 8, CMOS_TIMER_PORT_0);

    /* CMOS chip is already programmed with a slow but acceptable
       interval timer. Just use that. */
    irq_BindVector(iv_Legacy_PIT, pit_wakeup);
    irq_EnableVector(iv_Legacy_PIT);
  }
}
