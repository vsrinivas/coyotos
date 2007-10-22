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
 * global descriptor table.
 */

#include <stddef.h>
#include <hal/kerntypes.h>
#include <kerninc/ccs.h>
#include <kerninc/string.h>
#include <kerninc/CPU.h>

#include "IA32/GDT.h"
#include "IA32/TSS.h"

#include "kva.h"
#include "GDT.h"
#include "TSS.h"
#include "Selector.h"

typedef uint32_t GDT_t[gseg_NGDT*2];

/**
 * @brief Global Descriptor Table.
 *
 * Vector of per-CPU GDT structures. Coyotos currently keeps a
 * distinct GPT structure for each CPU in order to eliminate locking
 * overheads. The content of each Global Descriptor Table is mostly
 * constant and mostly CPU-neutral, with the exceptions of the TSS
 * entry and the user TLS entry. The TSS entry points to the per-CPU
 * TSS (necessary to get correct stack pointer on kernel entry). The
 * User TLS entry points to the TLS area for the application currently
 * running on this CPU.
 *
 * We presently hand-initialize the first entry in the table and copy
 * that into subsequent entries, mainly because a purely static
 * initialization lends itself to update errors.
 *
 * The application thread-local storage entry is used to tell the
 * application (via <b>%%FS</b>) the base of its thread-local storage,
 * and this entry is overwritten before each process dispatch. In
 * consequence, the GDT structure has been made per-CPU to eliminate
 * any need for locking.
 *
 * If you add or rearrange entries to this structure, be sure to
 * update the entries in Selector.h accordingly.
 */
GDT_t gdtTables[MAX_NCPU] = {

  {
				/* Entry 0 - Null Segment */
    0x0,
    0x0,

				/* Entry 1 - TSS descriptor */
    sizeof(ia32_TSS)-1,		/* 104 bytes, base at XXXX */
    0x00008900,			/* TSS,DPL0,byte granularity, base at XXXX */

    /* Because of the specification of Intel's SYSENTER/SYSEXIT
       instructions, these segments MUST appear in the following order:

       kernel CS
       kernel DS
       user   CS
       user   DS

       See the SYSEXIT instruction documentation to see the rules for
       how segment selectors are reloaded for details. */

				/* Entry 2 - Kernel Code */
    0x0000ffff,			/* 4G (for now), base at 0 */
    0x00cf9b00,			/* accessed, ReadExec, paged, 386, DPL0 */

				/* Entry 3 - Kernel Data/Stack */
    0x0000ffff,			/* 4G (for now), base at 0 */
    0x00cf9300,			/* accessed, ReadWrite, paged, 386, DPL0 */

				/* Entry 4 - User Code */
    0x0000ffff,			/* 4G (for now), base at 0 */
    0x00cffb00,			/* accessed, ReadExec, paged, 386, DPL3 */

				/* Entry 5 - User Data/Stack */
    0x0000ffff,			/* 4G (for now), base at 0 */
    0x00cff300,			/* accessed, ReadWrite, paged, 386, DPL3 */

				/* Entry 6 - User thread-local storage */
    0x0000ffff,			/* 4G (for now), base is variable */
    0x00cff300			/* accessed, ReadWrite, paged, 386, DPL3 */
  }
};

uint32_t LdtTable[] = {
				/* Entry 0 - Null Segment */
  0x0,
  0x0
};

/**
 * Initialize the per-CPU GDT structures.
 */
void
init_gdt()
{
  for (size_t i = 1; i < MAX_NCPU; i++)
    memcpy(&gdtTables[i], &gdtTables[0], sizeof(gdtTables[0]));

  for (size_t i = 0; i < MAX_NCPU; i++) {
    ia32_SegDescriptor *myGDT = (ia32_SegDescriptor *) &gdtTables[i];
    ia32_SegDescriptor* seg = myGDT + seg_TSS;

    kva_t linearBase = KVTOL((kva_t) &tss[i]);
  
    seg->loBase = linearBase & 0xffffu;
    seg->midBase = (linearBase >> 16) & 0xffu;
    seg->hiBase = (linearBase >> 24) & 0xffu;
  }
}

/**
 * @brief Initialize the GDTR register, reload all segment registers
 * to reference the new GDT table, then initialize LDTR.
 *
 * I would prefer to handle this in boot.S for CPU0, but doing it here
 * is soon enough, and this way is more robustly maintainable if we
 * need to add entries to the GDT table. We need this code in any case
 * so that we can initialize the second and subsequence CPUs on a
 * multiprocessor.
 */
void
load_gdtr_ldtr(void)
{
  cpuid_t myCPU = cpu_getMyID();

  typedef struct {
    uint32_t size : 16;
    uint32_t lowAddr : 16;
    uint32_t hiAddr;
  } DescriptorTablePointer;

  // Because GdtTable is an array, using *PERCPU(GdtTable) within the
  // sizeof() doesn't work here.
  DescriptorTablePointer GDTdescriptor = {
    sizeof(gdtTables[myCPU]),
    (uint32_t) KVTOL(gdtTables[myCPU]),
    ((uint32_t) KVTOL(gdtTables[myCPU])) >> 16 };

  GNU_INLINE_ASM("lgdt %0\n"
		 "ljmp %1,$1f\n"
		 "1: mov %2,%%ax\n"
		 "mov %%ax,%%ds\n"
		 "mov %%ax,%%es\n"
		 "mov %%ax,%%ss\n"
		 "xor %%ax,%%ax\n"
		 "mov %%ax,%%fs\n"
		 "mov %%ax,%%gs\n"
		 "lldt %%ax\n"
		 : /* no output */
		 : "m" (GDTdescriptor),
		 "i" (sel_KernelCode),
		 "i" (sel_KernelData)
		 : "memory", "ax");
}
