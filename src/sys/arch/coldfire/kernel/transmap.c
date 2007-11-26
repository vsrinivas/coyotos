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
 * The implementation here is specialized for soft TLB machines. We
 * create mappings starting at TRANSMAP_WINDOW_KVA, but we do not need
 * an actual page table to manage those mappings. A simple, statically
 * allocated vector of physical addresses suffices.
 */

#include <stddef.h>
#include <coyotos/coldfire/pagesize.h>
#include <hal/kerntypes.h>
#include <hal/transmap.h>
#include <hal/vm.h>
#include <kerninc/printf.h>
#include <kerninc/assert.h>
#include <kerninc/CPU.h>
#include <kerninc/string.h>

#include "hwmap.h"

#define TRANSMAP_PERCPU_ENTRY(slot) ((slot) + (MY_CPU(id) * TRANSMAP_ENTRIES_PER_CPU))
#define TRANSMAP_ENTRY_VA(slot) (TRANSMAP_WINDOW_KVA + (COYOTOS_PAGE_SIZE * (slot)))
#define TRANSMAP_VA_ENTRY(va) (((va) - TRANSMAP_WINDOW_KVA) / COYOTOS_PAGE_SIZE)
#define TRANSMAP_ENTRY_SLOT(entry) ((entry) % TRANSMAP_ENTRIES_PER_CPU)

/** Page table entries corresponding to the transmap. */
pte_t transMappings[TRANSMAP_ENTRIES_PER_CPU];

#define DEBUG_TRANSMAP if (0)

void
transmap_init()
{
  for (size_t i = 0; i < sizeof(transMappings); i++)
    pte_invalidate(&transMappings[i]);
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
  kva_t va = TRANSMAP_ENTRY_VA(entry);

  transMappings[entry].dr.value  = pa;
  transMappings[entry].dr.bits.R = 1;
  transMappings[entry].dr.bits.W = 1;
  transMappings[entry].dr.bits.SZ = COLDFIRE_PAGE_SIZE_8K;
  transMappings[entry].dr.bits.CM = COLDFIRE_CACHE_WRITEBACK;
  transMappings[entry].dr.bits.SP = 1;

  assert(transMappings[entry].dr.bits.X == 0);
  assert(transMappings[entry].dr.bits.resvd == 0);
  assert(transMappings[entry].dr.bits.LK == 0);

  transMappings[entry].tr.value  = va;
  transMappings[entry].tr.bits.V  = 1; /* valid */

  assert(transMappings[entry].tr.bits.SG == 0);
  assert(transMappings[entry].tr.bits.asid  == 0); /* supervisor */


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

  pte_invalidate(&transMappings[entry]);

  // Defer the flush.
  MY_CPU(TransReleased) |= (1u << slot);

  DEBUG_TRANSMAP
    printf("Transmap: unmap va=0x%08x\n", va);

  return;
}
