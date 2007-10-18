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
#include <kerninc/Interval.h>
#include <coyotos/i386/io.h>
#include "PIC.h"
#include "IRQ.h"
#include "lapic.h"

// #define USE_LAPIC
#define DEBUG_HARDCLOCK if (0)


#define CMOS_HARD_TICK_RATE     1193182L
#define CMOS_READ_GUARD         16
#define BASETICK                200 /* millisecond interrupts - QEMU */

/* CMOS_TICK_DIVIDER is the divider to use to get the soft tick rate down
 * as close as possible to the value specified by BASETICK. The soft tick
 * rate MUST be >= BASETICK.
 * 
 * Given a value for CMOS_TICK_DIVIDER, the number of ticks per second
 * is given by HARD_TICK_RATE/CMOS_TICK_DIVIDER, and the number of ticks
 * per millisecond is (HARD_TICK_RATE/CMOS_TICK_DIVIDER) * (1/1000).
 * Here's where it all starts to become interesting.  
 * 
 * Since we know the clock rate, getting second intervals out of the
 * CMOS tick rate is not difficult. We then need to convert the
 * residual ticks to microseconds using 32-bit integer arithmetic. In
 * essence, we need to compute (1000000/1193182)(residual) within
 * reasonable accuracy. We do this by multiplying by (50000/59659),
 * which introduces a small, non-cumulative round-off error.
 *
 * We need to convert milliseconds to ticks using integer arithmetic. 
 * One year is longer than we are going to go without a reboot (random
 * alpha hits, if nothing else), so we set that as an upper bound on the
 * sleep call.  One year is just less than 2^35 milliseconds, and 64 bit
 * integer arithmetic is the largest convenient kind on this machine.
 * What we're going to do is take the milliseconds number handed us,
 * multiply that by 2^20 * (ticks/ms), and then divide the whole mess
 * by 2^20 giving ticks.  The end result will be to correct for tick
 * drift.
 * 
 * TICK_MULTIPLIER, then, is the result of computing (by hand):
 * 	2^20 * (HARD_TICK_RATE/(CMOS_TICK_DIVIDER*1000))
 * 
 * Note: if you use UNIX bc, compute these with scale=8
 * 
 * The other conversion we need to be able to do is to go from ticks
 * since last reboot to milliseconds.  This is basically the same
 * calculation done in the opposite direction.  The goal is not to be
 * off by more than 1ms per 1024*1024 seconds.
 * 
 * TICK_TO_MS_MULTIPLIER, then, is the result of computing (by hand):
 *      2^20 * ((1000 * 2^20) / (INTERRUPT_RATE * 2^20))
 *  where
 *    INTERRUPT_RATE = HARD_TICK_RATE / CMOS_TICK_DIVIDER
 */

#if (BASETICK == 4000)
# define CMOS_TICK_DIVIDER	298
# define TICK_MULTIPLIER	4198463ll
# define TICK_TO_MS_MULTIPLIER  261884ll
#elif (BASETICK == 2000)
# define CMOS_TICK_DIVIDER	596
# define TICK_MULTIPLIER	2099231ll
# define TICK_TO_MS_MULTIPLIER  523768ll
#elif (BASETICK == 1000)
# define CMOS_TICK_DIVIDER	1193
# define TICK_MULTIPLIER	1048736ll
# define TICK_TO_MS_MULTIPLIER  1048416ll
#elif (BASETICK == 500)
# define CMOS_TICK_DIVIDER	2386
# define TICK_MULTIPLIER	524368ll
# define TICK_TO_MS_MULTIPLIER  2096832ll
#elif (BASETICK == 200)
# define CMOS_TICK_DIVIDER	5965
# define TICK_MULTIPLIER	209747ll
# define TICK_TO_MS_MULTIPLIER  5242080ll
#elif (BASETICK == 100)
# define CMOS_TICK_DIVIDER	11931
# define TICK_MULTIPLIER	104865ll
# define TICK_TO_MS_MULTIPLIER  10485039ll
#elif (BASETICK == 60)
# define CMOS_TICK_DIVIDER	19886
# define TICK_MULTIPLIER	62912ll
# define TICK_TO_MS_MULTIPLIER  17475944ll
#elif (BASETICK == 50)
# define CMOS_TICK_DIVIDER	23863
# define TICK_MULTIPLIER	52430ll
# define TICK_TO_MS_MULTIPLIER  20991457ll
#else
# error "BASETICK not properly defined"
#endif


#define CMOS_TIMER_PORT_0	0x40
#define CMOS_TIMER_MODE		0x43
#define CMOS_SQUARE_WAVE0	0x34

uint64_t cmos_ticks_since_start;
uint32_t cmos_last_count;

static inline uint16_t cmos_read_tick()
{
  uint16_t cmos_hi;
  uint16_t lo, hi;
  do {
    lo = inb(CMOS_TIMER_PORT_0);
    hi = inb(CMOS_TIMER_PORT_0);
    cmos_hi = (hi <<8) | lo;
  } while (lo < CMOS_READ_GUARD);
  return cmos_hi;
}

/** @brief CMOS PIT wakeup handler.
 *
 * The assumption here is that we will never miss a tick!
 */
static void 
cmos_pit_wakeup(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
{
  uint32_t last = cmos_last_count;
  cmos_last_count = cmos_read_tick();

  /* CMOS uses a 16 bit counter. Deal with underflow: */
  if (last < cmos_last_count) last += 0x10000;
  uint32_t delta = last - cmos_last_count;

  cmos_ticks_since_start += delta;

  Interval curTick;

  curTick.sec = cmos_ticks_since_start / CMOS_HARD_TICK_RATE;
  curTick.usec = cmos_ticks_since_start % CMOS_HARD_TICK_RATE;
  curTick.usec *= 50000;
  curTick.usec /= 59659;

  interval_update_now(curTick);

  /* Re-enable the periodic timer interrupt line: */
  vec->unmasked = 1;
  vec->pending = 0;
  vec->ctrlr->unmask(vec);

  //  printf("%s Timer Interrupt!\n", inProc ? "Process" : "Kernel");

  /* Preemption has occurred. */
  if (inProc) {
    LOG_EVENT(ety_UserPreempt, inProc, 0, 0);
    atomic_set_bits(&CUR_CPU->flags, CPUFL_WAS_PREEMPTED);
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
  if (MY_CPU(current) &&
      ((atomic_read(&CUR_CPU->flags) & CPUFL_WAS_PREEMPTED) == 0)) {
    /* If the timer is free-running, it may be set in motion before
     * there is any process on the current CPU. In that case there is
     * nothing to preempt yet.
     */
    atomic_set_bits(&MY_CPU(current)->issues, pi_Preempted);
    atomic_set_bits(&CUR_CPU->flags, CPUFL_WAS_PREEMPTED);

    LOG_EVENT(ety_KernPreempt, MY_CPU(current), 0, 0);
    DEBUG_HARDCLOCK
      printf("Current process preempted by timer (from %s).\n",
	     inProc ? "user" : "kernel");
  }

  return;
}

static void cmos_interval_init()
{
  outb(CMOS_SQUARE_WAVE0, CMOS_TIMER_MODE);
  outb(CMOS_TICK_DIVIDER & 0xffu, CMOS_TIMER_PORT_0);
  outb(CMOS_TICK_DIVIDER >> 8, CMOS_TIMER_PORT_0);

  /* CMOS chip is already programmed with a slow but acceptable
     interval timer. Just use that. */
  irq_Bind(irq_ISA_PIT, VEC_MODE_FROMBUS, VEC_LEVEL_FROMBUS, cmos_pit_wakeup);

  cmos_ticks_since_start = 0;
  cmos_last_count = cmos_read_tick();

  VectorInfo *vector = irq_MapInterrupt(irq_ISA_PIT);
  VectorHoldInfo vhi = vector_grab(vector);

  vector->pending = 0;
  vector->unmasked = 1;
  vector->ctrlr->unmask(vector);

  vector_release(vhi);
}


#ifdef USE_LAPIC
static uint32_t
lapic_calibrate()
{
#define CALIBRATE_TICKS 2500
#define CALIBRATE_GUARD         50

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
  do {
    cmos_hi = cmos_read_tick();
  } while (cmos_hi < (CALIBRATE_TICKS+CALIBRATE_GUARD) );

  /* Get a starting read from the CMOS clock. */
  cmos_hi = cmos_read_tick();

  /* Get a starting read from the LAPIC timer. */
  uint32_t lapic_hi = lapic_read_register(LAPIC_TIMER_CCR);

  uint32_t cmos_lo = cmos_hi;

  /* Watch the CMOS clock for a while. */
  do {
    cmos_lo = cmos_read_tick();
  } while ((cmos_hi - cmos_lo) < CALIBRATE_TICKS);

  uint32_t lapic_lo = lapic_read_register(LAPIC_TIMER_CCR);

  uint32_t ratio = (lapic_hi - lapic_lo) / (cmos_hi - cmos_lo);

  DEBUG_HARDCLOCK
    printf("%d CMOS ticks => %d lapic_ticks (%d)\n", 
	   (cmos_hi - cmos_lo), (lapic_hi - lapic_lo), 
	   ratio);
  return ratio;
}

static void 
lapic_interval_init()
{
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
  
  DEBUG_HARDCLOCK
    printf("LVT TIMER: 0x%08x\n", lapic_read_register(LAPIC_LVT_Timer));

  irq_Bind(irq_LAPIC_Timer, VEC_MODE_FROMBUS, VEC_LEVEL_FROMBUS, lapic_pit_wakeup);

  lapic_write_register(LAPIC_TIMER_ICR, ratio * CMOS_TICK_DIVIDER);
  irq_Enable(irq_LAPIC_Timer);
}
#endif

void
hardclock_init()
{
  // For testing, set things up to use the legacy PIT even in APIC mode.
#ifdef USE_LAPIC
  if (use_ioapic && lapic_pa) {
    lapic_interval_init();
  }
  else
#endif
    cmos_interval_init();
}
