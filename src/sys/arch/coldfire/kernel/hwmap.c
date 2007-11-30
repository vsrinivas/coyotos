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
 * @brief Manipulation of the hardware mapping table.
 */

#include <stddef.h>
#include <kerninc/assert.h>
#include <kerninc/printf.h>
#include <kerninc/Depend.h>
#include <kerninc/Mapping.h>
#include <kerninc/RevMap.h>
#include <kerninc/event.h>
#include <kerninc/AgeList.h>
#include <hal/transmap.h>

#include "hwmap.h"

/* This lock is not strictly required, but is preserved for
 * parallelism with other architectures, and also because somebody may
 * do a coldfire multiprocessor someday.
 */
spinlock_t mappingListLock;

Mapping KernMapping;
Mapping UserMapping[256];	/* One per user ASID. Entry 0 is never used */

AgeList MappingAgeList = { { &MappingAgeList.list, &MappingAgeList.list, } } ;

/* Following are placeholder implementations */
void
global_tlb_flush()
{
  local_tlb_flush();
}

void
global_tlb_flushva(kva_t va)
{
  local_tlb_flush();
}

static inline void
tlb_flush_asid(uint8_t asid)
{
  /* There is no easy way to flush the TLB without flushing shared
   * entries, which is a nuisance. */
  hwreg_write(COLDFIRE_MMUTR, (asid << 2));
  hwreg_write(COLDFIRE_MMUOR, COLDFIRE_MMUOR_CAS);
}


void
pagetable_init(void)
{
  printf("Initializing mappings\n");

  link_init(&KernMapping.ageLink);
  KernMapping.asid = 0;
  MY_CPU(curMap) = &KernMapping;

  for (size_t i = 1; i < 256; i++) {
    link_init(&UserMapping[i].ageLink);
    UserMapping[i].asid = i;
    agelist_addBack(&MappingAgeList, &UserMapping[i]);
  }
}

/** @brief Make the mapping @p m undiscoverable by removing it from
 * its product chain. 
 *
 * @bug This should be generic code, but it isn't obvious what source
 * file to stick it in.
 */
static void
mapping_make_unreachable(Mapping *m)
{
  /* Producer chains are guarded by the mappingListLock. Okay to
     fiddle them here. */

  MemHeader *hdr = m->producer;
  Mapping **mPtr = &hdr->products;

  while (*mPtr != m)
    mPtr = &((*mPtr)->nextProduct);

  /* We really *should* actually be *on* the product list: */
  assert(*mPtr == m);

  *mPtr = m->nextProduct;
}

static Mapping *
mapping_alloc()
{
  assert(spinlock_isheld(&mappingListLock));

  Mapping *map;
  for(;;) {
    map = agelist_oldest(&MappingAgeList);

    if (map->producer) {
      /* Make this page table undiscoverable. */
      mapping_make_unreachable(map);

      rm_whack_mapping(map);
    }

    break;
  }

  assert(map);

  /* There is no need to re-initialize the mapping table, since we
     don't presently HAVE an explicit mapping table, but we do need to
     flush the corresponding ASID from the TLB */

  tlb_flush_asid(map->asid);

  /* Newly allocated, so move to front. */
  agelist_remove(&MappingAgeList, map);
  agelist_addFront(&MappingAgeList, map);

  return map;
}

Mapping *
mapping_get(MemHeader *hdr,
	    coyaddr_t guard, coyaddr_t mask, size_t restr)
{
  SpinHoldInfo shi = spinlock_grab(&mappingListLock);

  for (Mapping *cur = hdr->products;
       cur != NULL;
       cur = cur->nextProduct) {
    if (cur->match == guard &&
	cur->mask == mask &&
	cur->restr == restr) {

      agelist_remove(&MappingAgeList, cur);
      agelist_addFront(&MappingAgeList, cur);

      spinlock_release(shi);
      return cur;
    }
  }
  
  Mapping *nMap = mapping_alloc();

  nMap->match = guard;
  nMap->mask = mask;
  nMap->restr = restr;

  nMap->producer = hdr;
  nMap->nextProduct = hdr->products;
  hdr->products = nMap;

  spinlock_release(shi);
  return nMap;
}

void
memhdr_destroy_products(MemHeader *hdr)
{
  SpinHoldInfo shi = spinlock_grab(&mappingListLock);

  Mapping *map;

  while ((map = hdr->products) != NULL) {
    /* Make this page table undiscoverable. */
    mapping_make_unreachable(map);

    rm_whack_mapping(map);

    agelist_remove(&MappingAgeList, map);
    agelist_addBack(&MappingAgeList, map);
  }

  spinlock_release(shi);
}

void
depend_entry_invalidate(const DependEntry *entry, int slot)
{
  global_tlb_flush();
}

void
rm_whack_pte(struct Mapping *map,  size_t slot)
{
  global_tlb_flush();
}

void
rm_whack_process(Process *proc)
{
  HoldInfo hi = mutex_grab(&proc->hdr.lock);
  proc->mappingTableHdr = 0;
  if (proc == MY_CPU(current))
    vm_switch_curcpu_to_map(&KernMapping);
  mutex_release(hi);
}

#if 0
void
hwmap_enable_low_map()
{
  if (UsingPAE)
    KernPDPT.entry[0] = KernPDPT.entry[3];
  else
    KernPageDir[0] = KernPageDir[768];
}

void
hwmap_disable_low_map()
{
  if (UsingPAE)
    PTE_CLEAR(KernPDPT.entry[0]);
  else
    PTE_CLEAR(KernPageDir[0]);
}
#endif

