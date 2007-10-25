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
 * @brief Management of the transient map.
 *
 * Manages up to 64 simultaneous transient mappings. 64 is not a magic
 * number. It simply happens to be more than we actually need, and can
 * be conveniently packed into a long long.
 *
 * This implementation hurts us at machine startup time, because we
 * see a lot of map reuse during heap construction, and possibly
 * during boot-time page directory and page table allocation.
 */

#include <coyotos/i386/pagesize.h>
#include <hal/transmap.h>
#include <hal/kerntypes.h>
#include <hal/vm.h>
#include <kerninc/string.h>
#include <kerninc/printf.h>
#include <kerninc/assert.h>

#include "hwmap.h"
#include "kva.h"

#define TRANSMAP_PERCPU_ENTRY(slot) ((slot) + (MY_CPU(id) * TRANSMAP_ENTRIES_PER_CPU))
#define TRANSMAP_ENTRY_VA(slot) (TRANSMAP_WINDOW_KVA + (COYOTOS_PAGE_SIZE * (slot)))
#define TRANSMAP_VA_ENTRY(va) (((va) - TRANSMAP_WINDOW_KVA) / COYOTOS_PAGE_SIZE)
#define TRANSMAP_ENTRY_SLOT(entry) ((entry) % TRANSMAP_ENTRIES_PER_CPU)

/* The transient map is mapped comonly on all CPUs, but each CPU
   accesses a disjoint subregion of it. */

extern kva_t TransientMap[];

#define DEBUG_TRANSMAP if (0)

void
transmap_init()
{
  memset(TransientMap, 0, COYOTOS_PAGE_SIZE * TRANSMAP_PAGES);

  for (size_t i = 0; i < TRANSMAP_PAGES; i++) {
    const uint32_t offset = COYOTOS_PAGE_SIZE * i;
    const kva_t va = TRANSMAP_WINDOW_KVA + offset;
    const uint32_t pa = (((uint32_t)&TransientMap) - KVA) + offset;

    if (UsingPAE) {
      uint32_t undx = PAE_PGDIR_NDX(va);
      IA32_PAE *upper = ((IA32_PAE*) &KernPageDir) + undx;

      upper->value = pa;
      upper->bits.W = 1;
      upper->bits.V = 1;
      upper->bits.ACC = 1;
      upper->bits.DIRTY = 1;
      upper->bits.PGSZ = 0;
    }
    else {
      uint32_t undx = PTE_PGDIR_NDX(va);
      IA32_PTE *upper = ((IA32_PTE*) &KernPageDir) + undx;

      upper->value = pa;
      upper->bits.W = 1;
      upper->bits.V = 1;
      upper->bits.ACC = 1;
      upper->bits.DIRTY = 1;
      upper->bits.PGSZ = 0;
    }
  }
}

void
transmap_advise_tlb_flush()
{
  DEBUG_TRANSMAP printf("Transmap: flushed\n");

  MY_CPU(TransMetaMap) |= MY_CPU(TransReleased);
  MY_CPU(TransReleased) = 0;
}

kva_t
transmap_map(kpa_t pa)
{
  assert((pa % COYOTOS_PAGE_SIZE) == 0);
  assert(pa < PAE_PADDR_BOUND);

  uint32_t ndx = ffsll(MY_CPU(TransMetaMap));
  if (ndx == 0) {
    /* Grab back just one at a time, and use selective flush
     * instruction. This works much better on emulators.
     */
    ndx = ffsll(MY_CPU(TransReleased));
    assert(ndx);
    uint32_t slot = ndx-1;
    uint32_t entry = TRANSMAP_PERCPU_ENTRY(slot);
    MY_CPU(TransReleased) &= ~(1u << slot);
    local_tlb_flushva(TRANSMAP_ENTRY_VA(entry));
  }

  assert(ndx);

  uint32_t slot = ndx - 1;
  uint32_t entry = TRANSMAP_PERCPU_ENTRY(slot);

  if (UsingPAE) {
    IA32_PAE *theMap = (IA32_PAE *) &TransientMap;
    IA32_PAE *theEntry = &theMap[entry];

    PAE_CLEAR(*theEntry);
    theEntry->value = pa;
    theEntry->bits.V = 1;
    theEntry->bits.W = 1;
    theEntry->bits.ACC = 1;
    theEntry->bits.DIRTY = 1;
  }
  else {
    IA32_PTE *theMap = (IA32_PTE *) &TransientMap;
    IA32_PTE *theEntry = &theMap[entry];

    PTE_CLEAR(*theEntry);
    theEntry->value = pa;
    theEntry->bits.V = 1;
    theEntry->bits.W = 1;
    theEntry->bits.ACC = 1;
    theEntry->bits.DIRTY = 1;
  }

  kva_t va = TRANSMAP_ENTRY_VA(entry);
  MY_CPU(TransMetaMap) &= ~(1u << slot);

  DEBUG_TRANSMAP
    printf("Transmap: map va=0x%08x to pa=0x%016x\n", va, pa);

  return va;
}

void 
transmap_unmap(kva_t va)
{
  uint32_t entry = TRANSMAP_VA_ENTRY(va);
  uint32_t slot = TRANSMAP_ENTRY_SLOT(entry);

  assert ((MY_CPU(TransMetaMap) & (1u << slot)) == 0);
  assert ((MY_CPU(TransReleased) & (1u << slot)) == 0);

  if (UsingPAE) {
    IA32_PAE *theMap = (IA32_PAE *) &TransientMap;
    IA32_PAE *theEntry = &theMap[entry];
    theEntry->value = 0;
  }
  else {
    IA32_PTE *theMap = (IA32_PTE *) &TransientMap;
    IA32_PTE *theEntry = &theMap[entry];
    theEntry->value = 0;
  }

  // Defer the flush.
  MY_CPU(TransReleased) |= (1u << slot);

  DEBUG_TRANSMAP
    printf("Transmap: unmap va=0x%08x\n", va);

  return;
}
