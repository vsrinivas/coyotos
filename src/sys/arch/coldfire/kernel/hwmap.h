#ifndef __COLDFIRE_HWMAP_H__
#define __COLDFIRE_HWMAP_H__
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
 */
#include <hal/kerntypes.h>
#include <kerninc/Process.h>

#define PTE_KPA_TO_FRAME(pa) (((uint32_t)(pa)) >> 13)
#define PTE_FRAME_TO_KPA(frm) ((frm) << 13)
#define PTE_CLEAR(pte) \
  do {							\
    (pte).dr.value = 0;					\
    (pte).tr.value = 0;					\
  } while (0)

#define KPA_IS_PAGE_ADDRESS(pa) ((pa & ((kpa_t)0x1fff)) == 0)
#define KVA_IS_PAGE_ADDRESS(va) ((va & ((kva_t)0x1fff)) == 0)

void local_tlb_flush();
void local_tlb_flushva(kva_t va);

/** @brief Re-establish the kernel mapping for the least 4M (PAE: 2M)
 * of physical memory. */
void hwmap_enable_low_map();

/** @brief Erase the kernel mapping for the least 4M (PAE: 2M)
 * of physical memory. */
void hwmap_disable_low_map();

/* Number of page tables to reserve for a given number of pages. */
#define RESERVED_PAGE_TABLES(nPage) 0
#define PAGES_PER_PROCESS 25

void do_pageFault(Process *base, uintptr_t addr, 
		  bool wantWrite, bool wantExec, bool wantCap);

#endif /* __COLDFIRE_HWMAP_H__ */
