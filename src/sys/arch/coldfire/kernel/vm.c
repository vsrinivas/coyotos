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
 * @brief Management and implementation of primary kernel map
 *
 * This implementation assumes that we are NOT doing PAE mode.
 */

#include <hal/kerntypes.h>
#include <hal/vm.h>
#include <hal/transmap.h>
#include <kerninc/assert.h>
#include <kerninc/string.h>
#include <kerninc/Mapping.h>
#include <kerninc/printf.h>
#include <kerninc/event.h>
#include <kerninc/shellsort.h>
#include "hwmap.h"

#define DEBUG_VM if (0)

#define SOFTTLB_MAXMAP 32
static size_t nMap = 0;

typedef struct SoftMap {
  kva_t va;
  kva_t bound;
  kpa_t pa;
  uint32_t perms;
} SoftMap;

static SoftMap kmap[SOFTTLB_MAXMAP];

void 
kmap_EnsureCanMap(kva_t va, const char *descrip)
{
  /* This is a soft TLB machine, so nothing to do here. */
}

static int 
cmp_kmap(const void *vp1, const void *vp2)
{
  const SoftMap *rgn1 = vp1;
  const SoftMap *rgn2 = vp2;

  if (rgn1->va < rgn2->va) return -1;
  if (rgn1->va > rgn2->va) return 1;
  return 0;
}

SoftMap *
kmap_search(kva_t va)
{
  for (size_t i = 0; i < nMap; i++)
    if (kmap[i].va <= va && va < kmap[i].bound)
      return &kmap[i];

  return NULL;
}

void 
kmap_map(kva_t va, kpa_t pa, uint32_t perms)
{
  assert (kmap_search(va) == 0);

  if (nMap == SOFTTLB_MAXMAP)
    fatal("Too many kernel mappings.\n");

  kmap[nMap].va = va;
  kmap[nMap].bound = va + COYOTOS_PAGE_SIZE;
  kmap[nMap].pa = va;
  kmap[nMap].perms = perms;

  nMap++;

  shellsort(kmap, SOFTTLB_MAXMAP, sizeof(*kmap), cmp_kmap);

  for (size_t i = 0; i < (nMap-1); i++) {
    if (kmap[i].perms != kmap[i+1].perms)
      continue;

    if ((kmap[i].bound == kmap[i+1].va) &&
	((kmap[i].pa + (kmap[i].bound = kmap[i].va)) ==
	 kmap[i+1].pa)) {
      kmap[i].bound = kmap[i+1].bound;

      for (size_t j = i+1; j < (nMap - 1); j++)
	kmap[j] = kmap[j+1];

      i--;
      nMap--;
    }
  }
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
  extern Mapping KernMapping;

  assert(map);
  assert((map == &KernMapping) ||
	 (MY_CPU(current) &&
	  map == MY_CPU(current)->mappingTableHdr &&
	  mutex_isheld(&MY_CPU(current)->hdr.lock)));
  // assert();

  LOG_EVENT(ety_MapSwitch, MY_CPU(curMap), map, 0);

  if (map == MY_CPU(curMap))
    return;

  MY_CPU(curMap) = map;

  local_tlb_flush();

  transmap_advise_tlb_flush();
}

