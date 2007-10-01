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
 * @brief Data structures and initialization code for the Pentium
 * task switch segmentd.
 */


#include <stddef.h>
#include <kerninc/ccs.h>
#include <kerninc/string.h>
#include <hal/config.h>

#include "IA32/TSS.h"

#include "TSS.h"
#include "GDT.h"
#include "Selector.h"

/**
 * @brief Task Switch Segment.
 *
 * Coyotos does not actually make much use of the TSS, but the
 * hardware requires it in order to have a place to get the kernel
 * stack pointer when we enter the kernel on a trap, hardware
 * interrupt, or INT instruction.
 *
 * In consequence, TSS is per-CPU.
 */

ia32_TSS tss[MAX_NCPU] = {
  {
    0,				/* backLink */
    0,				/* esp0 */
    sel_KernelData,		/* ss0 */
    0,				/* esp1 */
    sel_Null,		        /* ss1 */
    0,				/* esp2 */
    sel_Null,		        /* ss2 */
    0,				/* cr3 */
    0,				/* eip */
    0,				/* eflags */
    0,				/* eax */
    0,				/* ecx */
    0,				/* edx */
    0,				/* ebx */
    0,				/* esp */
    0,				/* ebp */
    0,				/* esi */
    0,				/* edi */
    0,				/* es */
    0,				/* cs */
    0,				/* ss */
    0,				/* ds */
    0,				/* fs */
    0,				/* gs */
    0,				/* ldtr */
    0,				/* trapOnSwitch */
    sizeof(ia32_TSS)		/* ioMapBase */
  }
} ;


void 
init_tss()
{
  for (size_t i = 1; i < MAX_NCPU; i++)
    memcpy(&tss[i], &tss[0], sizeof(tss[0]));
}

/**
 * @brief Load the TR register.
 */
void
load_tr()
{
  GNU_INLINE_ASM("ltr  %%ax"
		 : /* no output */
		 : "a" (sel_TSS)
		 );
}



