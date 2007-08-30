/*
 * Copyright (C) 2005, Jonathan S. Shapiro.
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
 * @brief Main kernel entry point.
 */

/** @mainpage Coyotos Kernel Source Code
 *
 * This is the Coyotos kernel source code, as processed using
 * Doxygen.  With a bit of luck, it provides a browsable view of the
 * code.
 */
#include <inttypes.h>
#include <hal/irq.h>
#include <kerninc/ccs.h>
#include <kerninc/capability.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/Process.h>
#include <kerninc/printf.h>
#include <kerninc/PhysMem.h>
#include <kerninc/Cache.h>
#include <kerninc/Sched.h>
#include <kerninc/assert.h>

#include <kerninc/MemWalk.h>

/** @brief Kernel entry point */
void
kernel_main(void)
{
  arch_init();

  obhdr_stallQueueInit();

  assert(local_interrupts_enabled());

  cache_init();

  assert(local_interrupts_enabled());

  arch_cache_init();

  assert(local_interrupts_enabled());

  printf("Dispatching first process...\n");

  sched_dispatch_something();

  fatal("Unexpected return from sched_dispatch_something\n");

  //  for(;;) {
  //    GNU_INLINE_ASM ("nop");
  //  }
}
