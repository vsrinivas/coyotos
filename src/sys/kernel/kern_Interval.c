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
 *
 * This is about the lamest sleep/wakeup implementation imaginable. We
 * make no attempt whatsoever to sort the sleepers by wake time. We
 * simply stick them on a stall queue, and whenever one is due to wake
 * up we wake all of them up and allow the hurd to thunder away.
 *
 * Needless to say, this implementation is a placeholder.
 */

#include <coyotos/coytypes.h>
#include <kerninc/Process.h>

/** @brief Guards manipulations of interval_now and interval_wakeup.
 *
 * A holder of this stall queue must lock out local interrupts as well as
 * grabbing the stall queue, so the correct protocol is to disable
 * interrupts locally first.
 */
static irqlock_t interval_irql = IRQLOCK_INIT;

/** @brief Current epoch, seconds and microseconds since boot. */
static Interval now = {0, 0, 0};

/** @brief Time when next wakeup needs to occur. */
static Interval wakeTime;

static DEFQUEUE(sleepers);

Interval
interval_now()
{
  Interval curNow;

  IrqHoldInfo ihi = irqlock_grab(&interval_irql);

  curNow = now;

  irqlock_release(ihi);

  return curNow;
}

void 
interval_update_now(Interval i)
{
  IrqHoldInfo ihi = irqlock_grab(&interval_irql);

  now = i;

  if (now.sec < wakeTime.sec ||
      (now.sec == wakeTime.sec && now.usec < wakeTime.usec)) {
    wakeTime.sec = 0;
    wakeTime.usec = 0;
    atomic_set_bits(&CUR_CPU->flags, CPUFL_NEED_WAKEUP);
  }

  irqlock_release(ihi);
}

void 
interval_do_wakeups()
{
  /* Must grab the interval_irql here, because otherwise we might get
     into a livelock with interval_delay. */
  IrqHoldInfo ihi = irqlock_grab(&interval_irql);

  sq_WakeAll(&sleepers, false);
  atomic_clear_bits(&CUR_CPU->flags, CPUFL_NEED_WAKEUP);

  irqlock_release(ihi);
}

void
interval_delay(Process *p)
{
  IrqHoldInfo ihi = irqlock_grab(&interval_irql);

  assert (p->wakeTime.epoch <= now.epoch);

  if (p->wakeTime.epoch >= now.epoch &&
      (p->wakeTime.sec > now.sec ||
       (p->wakeTime.sec == now.sec && p->wakeTime.usec > now.usec))) {

    if (p->wakeTime.sec < wakeTime.sec ||
	(p->wakeTime.sec == wakeTime.sec && p->wakeTime.usec < wakeTime.usec)) {
      
      wakeTime = p->wakeTime;

      /* Note that the following call involves a spinlock acquisition
       * that is technically unnecessary, because the sleeper stall
       * queue is already guarded by the irqlock. It isn't worth
       * optimizing until we have cause to do so.
       */
      sq_EnqueueOn(&sleepers);
    }

    irqlock_release(ihi);
  }
}
