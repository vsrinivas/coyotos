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

#include <kerninc/printf.h>
#include <kerninc/assert.h>
#include <kerninc/Sched.h>
#include <kerninc/ReadyQueue.h>
#include <kerninc/event.h>
#include <coyotos/i386/io.h>
#include "PIC.h"
#include "IRQ.h"
#include "lapic.h"

#define CMOS_HARD_TICK_RATE     1193182

// #define CMOS_TICK_DIVIDER	11931 /* (0x2e9b) ~10ms */
#define CMOS_TICK_DIVIDER	23863 /* (0x2e9b) ~50/sec */
#define CMOS_TIMER_PORT_0	0x40
#define CMOS_TIMER_MODE		0x43
#define CMOS_SQUARE_WAVE0	0x34

uint32_t
lapic_calibrate()
{
#define CALIBRATE_TICKS 2500
#define CALIBRATE_GUARD 50

  lapic_write_register(LAPIC_TIMER_DCR, LAPIC_DCR_DIVIDE_DIV2);
  lapic_write_register(LAPIC_LVT_Timer,
		       LAPIC_LVT_TIMER_PERIODIC|LAPIC_LVT_MASKED|
		       IRQ_PIN(irq_LAPIC_Timer));
  lapic_write_register(LAPIC_TIMER_ICR, 0xffffffff);

  /* Spin reading the CMOS clock until we know that (1) we did not
   * read during a borrow into the low byte from the high byte, and
   * (2) the value read is large enough that it will not underflow
   * while we are calibrating.
   */
  uint16_t cmos_hi;
  uint16_t lo, hi;
  do {
    lo = inb(CMOS_TIMER_PORT_0);
    hi = inb(CMOS_TIMER_PORT_0);
    cmos_hi = (hi <<8) | lo;
  } while (lo < 16 || cmos_hi < (CALIBRATE_TICKS+CALIBRATE_GUARD) );

  /* Get a starting read from the CMOS clock. */
  do {
    lo = inb(CMOS_TIMER_PORT_0);
    hi = inb(CMOS_TIMER_PORT_0);
    cmos_hi = (hi <<8) | lo;
  } while (lo < 16);

  /* Get a starting read from the LAPIC timer. */
  uint32_t lapic_hi = lapic_read_register(LAPIC_TIMER_CCR);

  uint32_t cmos_lo = cmos_hi;

  /* Watch the CMOS clock for a while. */
  do {
    lo = inb(CMOS_TIMER_PORT_0);
    hi = inb(CMOS_TIMER_PORT_0);
    cmos_lo = (hi <<8) | lo;
  } while (lo < 16 || (cmos_hi - cmos_lo) < CALIBRATE_TICKS);

  uint32_t lapic_lo = lapic_read_register(LAPIC_TIMER_CCR);

  uint32_t ratio = (lapic_hi - lapic_lo) / (cmos_hi - cmos_lo);

  printf("%d CMOS ticks => %d lapic_ticks (%d)\n", 
	 (cmos_hi - cmos_lo), (lapic_hi - lapic_lo), 
	 ratio);
  return ratio;
}

static void 
pit_wakeup(Process *inProc, fixregs_t *saveArea)
{
  VectorInfo *vector = &VectorMap[saveArea->ExceptNo];

  //  printf("%s Timer Interrupt!\n", inProc ? "Process" : "Kernel");

  /* Re-enable the periodic timer interrupt line: */
  irq_Enable(vector->irq);

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

#define USE_LAPIC
void
hardclock_init()
{
  // For testing, set things up to use the legacy PIT even in APIC mode.
  if (use_ioapic && lapic_pa) {
    uint64_t ratio = lapic_calibrate();

    uint64_t fsb_clock = ratio * ((uint64_t) CMOS_HARD_TICK_RATE) * 2;

    const uint64_t one_ghz = (1000ull * 1000ull * 1000ull);
    const uint64_t one_hundred_mhz = (1000ull * 1000ull * 100ull);
    const uint64_t one_mhz = (1000ull * 1000ull);

    if (fsb_clock > one_ghz) {
      uint64_t ghz = fsb_clock / one_ghz;
      fsb_clock -= (ghz * one_ghz);
      printf("FSB speed %d.%01d ghz\n", ghz,
	     fsb_clock / one_hundred_mhz);
    }
    else {
      printf("FSB speed %d mhz\n", fsb_clock/one_mhz);
    }

    uint32_t val = lapic_read_register(LAPIC_LVT_Timer);
    val &= ~LAPIC_LVT_TIMER_MODE;
    lapic_write_register(LAPIC_LVT_Timer, val | LAPIC_LVT_TIMER_PERIODIC|LAPIC_LVT_MASKED);

    printf("LVT TIMER: 0x%08x\n", lapic_read_register(LAPIC_LVT_Timer));

    irq_Bind(irq_LAPIC_Timer, VEC_MODE_FROMBUS, VEC_LEVEL_FROMBUS, pit_wakeup);

    lapic_write_register(LAPIC_TIMER_ICR, ratio * CMOS_TICK_DIVIDER);
#ifdef USE_LAPIC
    irq_Enable(irq_LAPIC_Timer);
#endif

#ifndef USE_LAPIC

    outb(CMOS_SQUARE_WAVE0, CMOS_TIMER_MODE);
    outb(CMOS_TICK_DIVIDER & 0xffu, CMOS_TIMER_PORT_0);
    outb(CMOS_TICK_DIVIDER >> 8, CMOS_TIMER_PORT_0);

    /* CMOS chip is already programmed with a slow but acceptable
       interval timer. Just use that. */
    irq_Bind(irq_ISA_PIT, VEC_MODE_FROMBUS, VEC_LEVEL_FROMBUS, pit_wakeup);
    irq_Enable(irq_ISA_PIT);
#endif
  }
  else {
    outb(CMOS_SQUARE_WAVE0, CMOS_TIMER_MODE);
    outb(CMOS_TICK_DIVIDER & 0xffu, CMOS_TIMER_PORT_0);
    outb(CMOS_TICK_DIVIDER >> 8, CMOS_TIMER_PORT_0);

    /* CMOS chip is already programmed with a slow but acceptable
       interval timer. Just use that. */
    irq_Bind(irq_ISA_PIT, VEC_MODE_FROMBUS, VEC_LEVEL_FROMBUS, pit_wakeup);
    irq_Enable(irq_ISA_PIT);
  }
}
