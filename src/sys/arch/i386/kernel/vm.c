/*
 * Copyright (C) 2005, The EROS Group, LLC.
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
 * @brief Management and implementation of primary kernel map
 *
 * This implementation assumes that we are NOT doing PAE mode.
 */

#include <hal/kerntypes.h>
#include <hal/vm.h>
#include <hal/transmap.h>
#include <kerninc/assert.h>
#include <kerninc/string.h>
#include <kerninc/PhysMem.h>
#include <kerninc/printf.h>
#include <kerninc/event.h>
#include "IA32/PTE.h"
#include "hwmap.h"

#define DEBUG_VM if (0)

void 
kmap_EnsureCanMap(kva_t va, const char *descrip)
{
  assert(va >= KVA);

  DEBUG_VM printf("kmap: Ensuring va=0x%08x\n", va);

  if (UsingPAE) {
    uint32_t undx = PAE_PGDIR_NDX(va);

    DEBUG_VM
      printf("Ensuring va=0x%08x undx is 0x%08x\n", va, undx);

    // If this is in fact a valid kernel address, it must be above the
    // kernel address threshold. In that case, it's top-level index
    // must be 3 and we can bypass directly to the master page
    // directory layer.
    assert(va >= KVA);
    assert(PAE_PDPT_NDX(va) == 3);

    IA32_PAE *upper = ((IA32_PAE*) &KernPageDir) + undx;

    assert(upper->bits.USER == 0);

    if (!upper->bits.V || upper->bits.PGSZ) {
      /// @bug After we are running, this needs to call
      /// heap_alloc_page_frame instead.
      kpa_t pa = pmem_AllocBytes(&pmem_need_pages, COYOTOS_PAGE_SIZE,
				 pmu_KMAP, descrip);

      DEBUG_VM
	printf("kmap: allocated page 0x%016x for page table\n", pa);

      IA32_PAE *pgtbl = TRANSMAP_MAP(pa, IA32_PAE *);

      if (upper->bits.PGSZ) {
	uint32_t frameno = upper->bits.frameno;
	for (size_t i = 0; i < NPAE_PER_PAGE; i++) {
	  pgtbl[i].bits = upper->bits;
	  pgtbl[i].bits.PGSZ = 0;
	  pgtbl[i].bits.frameno = frameno + i;
	}
      }
      else {
	memset(pgtbl, 0, COYOTOS_PAGE_SIZE);
      }

      TRANSMAP_UNMAP(pgtbl);

      /* Set up a sufficiently generic upper-level mapping entry for
	 all kernel uses. Specialized permissions on individual pages
	 will be determined at the leaf level. */
      upper->value = pa; /* this zeroes the permission bits! */
      upper->bits.V = 1;
      upper->bits.W = 1;
      upper->bits.ACC = 1;
      upper->bits.DIRTY = 1;
      upper->bits.PGSZ = 0;

      // FIX: This should be set if global mappings are supported
      // for this architecture:
      // upper->bits.GLBL = 1;
    }

    assert(upper->bits.USER == 0 &&
	   upper->bits.V == 1 &&
	   upper->bits.W == 1 &&
	   upper->bits.PGSZ == 0);

  }
  else {
    uint32_t undx = PTE_PGDIR_NDX(va);

    IA32_PTE *upper = ((IA32_PTE*) &KernPageDir) + undx;

    assert(upper->bits.USER == 0);

    if (!upper->bits.V || upper->bits.PGSZ) {
      // FIX: This needs to call heap_alloc_page_frame()
      kpa_t pa =
	pmem_AllocBytes(&pmem_need_pages, COYOTOS_PAGE_SIZE,
			pmu_KMAP, "heap map");

      IA32_PTE *pgtbl = TRANSMAP_MAP(pa, IA32_PTE *);

      if (upper->bits.PGSZ) {
	uint32_t frameno = upper->bits.frameno;
	for (size_t i = 0; i < NPTE_PER_PAGE; i++) {
	  pgtbl[i].bits = upper->bits;
	  pgtbl[i].bits.PGSZ = 0;
	  pgtbl[i].bits.frameno = frameno + i;
	}
      }
      else {
	memset(pgtbl, 0, COYOTOS_PAGE_SIZE);
      }

      TRANSMAP_UNMAP(pgtbl);

      /* Set up a sufficiently generic upper-level mapping entry for
	 all kernel uses. Specialized permissions on individual pages
	 will be determined at the leaf level. */
      upper->value = pa; /* this zeroes the permission bits! */
      upper->bits.V = 1;
      upper->bits.W = 1;
      upper->bits.ACC = 1;
      upper->bits.DIRTY = 1;
      upper->bits.PGSZ = 0;

      // FIX: This should be set if global mappings are supported
      // for this architecture:
      // upper->bits.GLBL = 1;
    }
  }
}

void 
kmap_map(kva_t va, kpa_t pa, uint32_t perms)
{
  /* Note that we do not purport to run on i386. PCD bit was supported
     on 486 and later, so no need to filter KMAP_NC. */

  assert(KPA_IS_PAGE_ADDRESS(pa));

  DEBUG_VM
    printf("kmap: Mapping va=0x%08x to pa=0x%016x\n", va, pa);

  if (UsingPAE) {
    uint32_t undx = PAE_PGDIR_NDX(va);
    uint32_t lndx = PAE_PGTBL_NDX(va);

    // If this is in fact a valid kernel address, it must be above the
    // kernel address threshold. In that case, it's top-level index
    // must be 3 and we can bypass directly to the master page
    // directory layer.
    assert(va >= KVA);
    assert(PAE_PDPT_NDX(va) == 3);

    IA32_PAE *upper = ((IA32_PAE*) &KernPageDir) + undx;

    assert(upper->bits.USER == 0 &&
	   upper->bits.V == 1 &&
	   upper->bits.W == 1 &&
	   upper->bits.PGSZ == 0);
 
    kpa_t lowerTable = PAE_FRAME_TO_KPA(upper->bits.frameno);
    IA32_PAE *pgtbl = TRANSMAP_MAP(lowerTable, IA32_PAE *);

    pgtbl[lndx].value = 0;
    // PTE_CLEAR(pgtbl[lndx]);

    pgtbl[lndx].bits.frameno = PAE_KPA_TO_FRAME(pa);

    pgtbl[lndx].bits.V = (perms ? 1 : 0);
    pgtbl[lndx].bits.W = (perms & KMAP_W) ? 1 : 0;
    pgtbl[lndx].bits.PCD = (perms & KMAP_NC) ? 1 : 0;
    pgtbl[lndx].bits.PGSZ = 0;
    pgtbl[lndx].bits.ACC = 1;
    pgtbl[lndx].bits.DIRTY = 1;
    pgtbl[lndx].bits.PGSZ = 0;

    // FIX: This should be set if global mappings are supported
    // for this architecture:
    // pgtbl[lndx].bits.GLBL = 1;

    TRANSMAP_UNMAP(pgtbl);
  }
  else {
    uint32_t undx = PTE_PGDIR_NDX(va);
    uint32_t lndx = PTE_PGTBL_NDX(va);

    IA32_PTE *upper = ((IA32_PTE*) &KernPageDir) + undx;

    assert(upper->bits.USER == 0 &&
	   upper->bits.V == 1 &&
	   upper->bits.W == 1 &&
	   upper->bits.PGSZ == 0);

    kpa_t lowerTable = PTE_FRAME_TO_KPA(upper->bits.frameno);

    IA32_PTE *pgtbl = TRANSMAP_MAP(lowerTable, IA32_PTE *);

    pgtbl[lndx].value = 0;
    // PTE_CLEAR(pgtbl[lndx]);

    pgtbl[lndx].bits.frameno = PTE_KPA_TO_FRAME(pa);

    pgtbl[lndx].bits.V = (perms ? 1 : 0);
    pgtbl[lndx].bits.W = (perms & KMAP_W) ? 1 : 0;
    pgtbl[lndx].bits.PCD = (perms & KMAP_NC) ? 1 : 0;
    pgtbl[lndx].bits.PGSZ = 0;
    pgtbl[lndx].bits.ACC = 1;
    pgtbl[lndx].bits.DIRTY = 1;
    pgtbl[lndx].bits.PGSZ = 0;

    // FIX: This should be set if global mappings are supported
    // for this architecture:
    // pgtbl[lndx].bits.GLBL = 1;

    TRANSMAP_UNMAP(pgtbl);
  }

  DEBUG_VM
    printf("kmap: Mapped va=0x%08x to pa=0x%016x\n", va, pa);
}

/// @brief Load the specified mapping onto the CPU.
///
/// This does not need to grab locks. If the mapping loaded is
/// KernMap, then no lock is needed. Any other mapping could be
/// whacked only through the RevMap, and that RevMap entry will be
/// marked REVMAP_TARGET_MAP_PROC, so any attempt to whack via that
/// root would attempt to grab our process lock and fail.
void
vm_switch_curcpu_to_map(Mapping *map)
{
  assert(map);
  assert(mutex_isheld(&MY_CPU(current)->hdr.lock));


  LOG_EVENT(ety_MapSwitch, MY_CPU(curMap), map, 0);

  if (map == MY_CPU(curMap))
    return;

  MY_CPU(curMap) = map;

  GNU_INLINE_ASM("mov %0,%%cr3"
		 : /* No output */
		 : "r" (map->pa));

  transmap_advise_tlb_flush();
}

