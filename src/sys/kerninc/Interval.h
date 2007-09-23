#ifndef __KERNINC_INTERVALCLOCK_H__
#define __KERNINC_INTERVALCLOCK_H__
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
 * @brief Shellsort.
 */

#include <coyotos/coytypes.h>
#include <stddef.h>

struct Process;

typedef struct Interval {
  uint32_t epoch;
  uint32_t sec;
  uint32_t usec;
} Interval;

/** @brief Update the current "time".
 *
 * If wakeup processing is needed, marks the current CPU state to
 * run the wakeup processing logic on the way out of the kernel.
 */
void interval_update_now(Interval);
void interval_do_wakeups();
Interval interval_now();
void interval_delay(struct Process *p);

#endif /* __KERNINC_INTERVALCLOCK_H__ */
