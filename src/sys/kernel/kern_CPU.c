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
 * @brief Per-CPU functions.
 */

#include <stdint.h>
#include <stdbool.h>
#include <kerninc/ccs.h>
#include <kerninc/CPU.h>
#include <hal/config.h>
#include <kerninc/mutex.h>
#include <kerninc/string.h>

CPU cpu_vec[MAX_NCPU] = {
};

/** @brief Number of identified CPUs. This may increase early in
 * arch-dependent initialization. */
size_t cpu_ncpu = 1;

void 
cpu_construct(cpuid_t ndx)
{
  CPU *cpu = &cpu_vec[ndx];

  INIT_TO_ZERO(cpu);
  cpu->id = ndx;
  cpu->procMutexValue = LOCKVALUE(0, LTY_TRAN, cpu->id);
}
