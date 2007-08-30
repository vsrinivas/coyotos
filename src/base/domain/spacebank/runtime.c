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
 * @brief SpaceBank runtime interaction file
 *
 * The job of this file is to mediate between SpaceBank and the runtime
 * system, mostly by having the runtime system get out of the way.
 */

#include "spacebank.h"
#include "coyotos.TargetInfo.h"

/** @brief Set up the stack pointer.
 *
 * @bug assumes that stacks grow downward.
 */
uintptr_t __rt_stack_pointer __attribute__((section(".data"))) =
  coyotos_TargetInfo_small_stack_pointer;

/** @brief prevent the runtime hook from foiling us */
uintptr_t __rt_runtime_hook __attribute__((section(".data"))) = 0;

/** @brief We never exit. */
int
atexit(void (*func)(void))
{
  return (0);
}

/** @brief We never exit. */
void
exit(int status)
{
  assert(0);
  for (;;)
    ;
}
