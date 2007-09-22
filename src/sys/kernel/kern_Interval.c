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

Interval now;
Interval wakeup;

DEFQUEUE(sleepers);

void 
interval_wakeup()
{
  if (now.sec < wakeup.sec)
    return;
  if (now.sec == wakeup.sec && now.usec < wakeup.usec)
    return;

  sq_WakeAll(&sleepers, false);
}

void
interval_delay(Process *p)
{
  assert (p->wakeTime.epoch <= now.epoch);

  if (p->wakeTime.epoch < now.epoch ||
      p->wakeTime.sec < now.sec ||
      (p->wakeTime.sec == now.sec && p->wakeTime.usec < now.usec))
    return;

  sq_EnqueueOn(&sleepers);
}
