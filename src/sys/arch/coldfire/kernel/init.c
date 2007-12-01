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
 * @brief Startup initialization for Coldfire.
 */

#include <hal/kerntypes.h>
#include <hal/console.h>
#include <kerninc/CPU.h>

void
arch_init()
{
  /* Initialize the console output first, so that we can get
   * diagnostics while the rest is running.
   */
  console_init();

  /* Initialize the CPU structures, since we need to call things that
   * want to grab mutexes.
   */
  for (size_t i = 0; i < MAX_NCPU; i++)
    cpu_construct(i);

}

void
arch_cache_init()
{
  //  process_modules();
}
