#ifndef __I386_HW_MAP_H__
#define __I386_HW_MAP_H__
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
 * @brief Macros for manipulating page table entries.
 *
 * Unusually, the PTE and PAE structures are declared in
 * target-hal/vm.h. This is necessary because we need those
 * declarations in order to declare TARGET_MAPROOT_T.
 */
#include <stdbool.h>
#include <stddef.h>
#include <coyotos/coytypes.h>
#include <hal/kerntypes.h>
#include <hal/vm.h>
#include <kerninc/ccs.h>
#include <kerninc/Process.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/CPU.h>
#include <kerninc/Mapping.h>

extern bool UsingPAE;
extern bool NXSupported;

#define NPTE_PER_PAGE (COYOTOS_PAGE_SIZE/sizeof(IA32_PTE))
#define PTE_PFRAME_BOUND (((kpa_t) 1) << 20)
#define PTE_PADDR_BOUND (((kpa_t) 1) << 32)
#define PTE_PGDIR_NDX(va) ((va) >> 22)
#define PTE_PGTBL_NDX(va) (((va) >> 12) % NPTE_PER_PAGE)
// When running in non-PAE mode, all PAs are representable in 32
// bits. We will often be handed a kpa_t as an argument to this macro,
// which is a 64-bit type. Downcast before shift to avoid un-needed
// shifts and masks.
#define PTE_KPA_TO_FRAME(pa) (((uint32_t)(pa)) >> 12)
#define PTE_FRAME_TO_KPA(frm) ((frm) << 12)
#define PTE_CLEAR(pte) (pte).value = 0


// The following should be used for the 64-bit extensions, but there
// is simply no way I'm going to try to build a single kernel binary
// that runs in either 64-bit or 32-bit mode. I don't know whether the
// hardware mapping system can be used in 4-layer mode while running a
// 32-bit supervisor, and it wouldn't really help us in any case,
// because the current limiting factor on physical memory is the
// available kernel virtual space to map the overhead structures.

// #define PAE_PFRAME_BOUND (((kpa_t) 1) << 51)
// #define PAE_PADDR_BOUND (((kpa_t) 1) << 63)

#define NPAE_PER_PAGE (COYOTOS_PAGE_SIZE/sizeof(IA32_PAE))
#define PAE_PFRAME_BOUND (((kpa_t) 1) << 24)
#define PAE_PADDR_BOUND (((kpa_t) 1) << 36)
#define PAE_PDPT_NDX(va) (((va) >> 30) % 4)
#define PAE_PGDIR_NDX(va) (((va) >> 21) % NPAE_PER_PAGE)
#define PAE_PGTBL_NDX(va) (((va) >> 12) % NPAE_PER_PAGE)
#define PAE_KPA_TO_FRAME(pa) ((pa) >> 12)
#define PAE_FRAME_TO_KPA(frm) ((frm) << 12)
#define PAE_CLEAR(pae) (pae).value = 0

#define PDPT_SIZE 0x4

#define KPA_IS_PAGE_ADDRESS(pa) ((pa & ((kpa_t)0xfff)) == 0)

extern IA32_PTE KernPageDir[];
extern uint32_t KernPageTable[];

typedef struct PDPT {
  IA32_PAE entry[PDPT_SIZE];
} PDPT;

extern PDPT cpu0_KernPDPT;

void local_tlb_flush();
void local_tlb_flushva(kva_t va);

/* Number of page tables to reserve for a given number of pages. */
#define RESERVED_PAGE_TABLES(nPage) (nPage / 10)
#define PAGES_PER_PROCESS 25

/** @brief Allocate a new page table, from free list if possible, else
 * by ageing.
 *
 * Returns a mapping structure that is appropriate for the requested
 * hardware mapping table level (0 == page table, 1 == page dir, 2 == PDPT).
 *
 * If the requested page table level is the highest level in use in
 * the current hardware implementation, the table returned will have
 * been initialized with the appropriate kernel mappings.
 */
struct Mapping *pgtbl_get(struct MemHeader *hdr, size_t level, 
			  coyaddr_t guard, coyaddr_t mask, size_t restr);


void do_pageFault(Process *base, uintptr_t addr, bool wantWrite, 
		  bool wantExec);

#endif /* __I386_HW_MAP_H__ */
