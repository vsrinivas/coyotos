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
 * @brief Manipulation of the hardware mapping table.
 */

#include <stddef.h>
#include <kerninc/assert.h>
#include <kerninc/printf.h>
#include <kerninc/Depend.h>
#include <kerninc/GPT.h>
#include <kerninc/PhysMem.h>
#include <kerninc/Cache.h>
#include <kerninc/Process.h>
#include <kerninc/malloc.h>
#include <kerninc/util.h>
#include <kerninc/AgeList.h>
#include <kerninc/pstring.h>
#include <kerninc/event.h>
#include "hwmap.h"
#include "kva.h"

PDPT KernPDPT;

spinlock_t mappingListLock;

Mapping *pageTable = 0;
Mapping *pdpt = 0;
AgeList ptAgeList = { { &ptAgeList.list, &ptAgeList.list, } } ;
AgeList pdptAgeList = { { &pdptAgeList.list, &pdptAgeList.list, } } ;

Mapping KernMapping;

/** @brief True if we are using PAE mode. Set in boot.S as we do
 * initial CPU feature probe.
 */
bool UsingPAE = false;
bool NXSupported = false;

void
local_tlb_flush()
{
  GNU_INLINE_ASM("mov %%cr3,%%eax\n"
		 "mov %%eax,%%cr3\n"
		 : /* No outputs */
		 : /* No inputs */
		 : "ax");
}

void
local_tlb_flushva(kva_t va)
{
  GNU_INLINE_ASM("invlpg %0\n"
		 : /* no output */
		 : "m" (*(char *)va)
		 : "memory"
		 );
}

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

static void
reserve_pgtbls(void)
{
  // Allocate physical storage for the page tables. I'm looking at
  // this and I'm really thinking that this is a terrible estimation
  // strategy, and we should grab these at need from the page heap...

  size_t nPage =
    pmem_Available(&pmem_need_bytes, COYOTOS_PAGE_SIZE, false);
  size_t nContig =
    pmem_Available(&pmem_need_bytes, COYOTOS_PAGE_SIZE, true);
  size_t nPageTableFrame = RESERVED_PAGE_TABLES(nPage);

  assert(nPageTableFrame <= nContig);

  kpa_t pa = 
    pmem_AllocBytes(&pmem_need_pages, COYOTOS_PAGE_SIZE * nPageTableFrame, 
		    pmu_PGTBL, "page tbls");

  pageTable = CALLOC(Mapping, nPageTableFrame);
  for (size_t i = 0; i < (nPageTableFrame - 1); i++) {
    pageTable[i].pa = pa + COYOTOS_PAGE_SIZE * i;
    link_init(&pageTable[i].ageLink);
    agelist_addBack(&ptAgeList, &pageTable[i]);
  }
}

static void
reserve_pdpts(void)
{
  if (!UsingPAE)
    return;

  assert(Cache.c_Process.count);

  size_t nPDPT = Cache.c_Process.count * 16;	/* pretty arbitrary */
  size_t nPDPTbytes = align_up(nPDPT * sizeof(PDPT), COYOTOS_PAGE_SIZE);
  nPDPT = nPDPTbytes / sizeof(PDPT);

  /* Grab them as physical pages, mainly because this guarantees
     pleasant alignment. */
  size_t nPDPTpages = nPDPTbytes / COYOTOS_PAGE_SIZE;

  /* PDPT structures are pointed to by hardware page directory
     register, which can only reference structures within the physical
     low 4G. Grab page-aligned start for best possible alignment. */
  PmemConstraint low4g = pmem_need_pages;
  low4g.bound = 0x0ffffffffllu;

  kpa_t pa = 
    pmem_AllocBytes(&low4g, COYOTOS_PAGE_SIZE * nPDPTpages, 
		    pmu_PGTBL, "PDPTs");

  pdpt = CALLOC(Mapping, nPDPT);
  for (size_t i = 0; i < nPDPT; i++) {
    pdpt[i].pa = pa + sizeof(PDPT) * i;
    link_init(&pdpt[i].ageLink);
    agelist_addBack(&pdptAgeList, &pdpt[i]);
  }
}

void 
pagetable_init(void)
{
  printf("Reserving page tables\n");
  reserve_pgtbls();
  reserve_pdpts();

  if (UsingPAE) {
    KernMapping.pa = KVTOP(&KernPDPT);
    KernMapping.level = 2;
  }
  else {
    KernMapping.pa = KVTOP(&KernPageDir);
    KernMapping.level = 1;
  }
  link_init(&KernMapping.ageLink);
  CUR_CPU->curMap = &KernMapping;
}

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


/// @bug Need to grab locks here!
void
vm_switch_curcpu_to_map(Mapping *map)
{
  assert(map);

  LOG_EVENT(ety_MapSwitch, CUR_CPU->curMap, map, 0);

  if (map == CUR_CPU->curMap)
    return;

  CUR_CPU->curMap = map;

  GNU_INLINE_ASM("mov %0,%%cr3"
		 : /* No output */
		 : "r" (map->pa));
  transmap_advise_tlb_flush();
}

/** @brief Make the mapping @p m undiscoverable by removing it from
 * its product chain. 
 *
 * @bug This should be generic code, but it isn't obvious what source
 * file to stick it in.
 */
static void
product_recall(Mapping *m)
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
alloc_mapping_table(size_t level)
{
  assert(spinlock_isheld(&mappingListLock));

  AgeList *ageList = (level == 2) ? &pdptAgeList : &ptAgeList;

  assert(level < 2 || UsingPAE);

  Mapping *pt;
  for(;;) {
    pt = agelist_oldest(ageList);

    if (pt->producer) {
      /* Make this page table undiscoverable. */
      product_recall(pt);

      rm_whack_mapping(pt);
    }

    break;
  }

  assert(pt);

  /* Re-initialize mapping table to safe state */
  if (level == 2) {
    memcpy_vtop(pt->pa, &KernPDPT, sizeof(KernPDPT));
    pt->userSlots = PDPT_SIZE - 1;
  } else if (level == 1 && !UsingPAE) {
    /* Re-initialize PDPT to safe state */
    memcpy_vtop(pt->pa, &KernPageDir, COYOTOS_PAGE_SIZE);
    pt->userSlots = PTE_PGDIR_NDX(KVA);
  } else {
    memset_p(pt->pa, 0, COYOTOS_PAGE_SIZE);
    pt->userSlots = UsingPAE ? NPAE_PER_PAGE : NPTE_PER_PAGE;
  }

  /* Newly allocated, so move to front. */
  agelist_remove(ageList, pt);
  agelist_addFront(ageList, pt);

  return pt;
}

Mapping *
pgtbl_get(MemHeader *hdr, size_t level, 
	  coyaddr_t guard, coyaddr_t mask, size_t restr)
{
  assert(level <= 1 + UsingPAE);

  SpinHoldInfo shi = spinlock_grab(&mappingListLock);

  for (Mapping *cur = hdr->products;
       cur != NULL;
       cur = cur->nextProduct) {
    if (cur->level == level &&
	cur->match == guard &&
	cur->mask == mask &&
	cur->restr == restr) {
      if (level == 2) {
	agelist_remove(&pdptAgeList, cur);
	agelist_addFront(&pdptAgeList, cur);
      } else {
	agelist_remove(&ptAgeList, cur);
	agelist_addFront(&ptAgeList, cur);
      }
      spinlock_release(shi);
      return cur;
    }
  }
  
  Mapping *nMap = alloc_mapping_table(level);

  nMap->level = level;
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
memhdr_invalidate_products(MemHeader *hdr)
{
  SpinHoldInfo shi = spinlock_grab(&mappingListLock);
  Mapping *pt;

  for (pt = hdr->products; pt != 0; pt = pt->nextProduct)
    rm_whack_mapping(pt);

  spinlock_release(shi);
}

void
memhdr_destroy_products(MemHeader *hdr)
{
  SpinHoldInfo shi = spinlock_grab(&mappingListLock);

  Mapping *pt;

  while ((pt = hdr->products) != NULL) {
    /* Make this page table undiscoverable. */
    product_recall(pt);

    rm_whack_mapping(pt);

    if (pt->level == 2) {
      agelist_remove(&pdptAgeList, pt);
      agelist_addBack(&pdptAgeList, pt);
    } else {
      agelist_remove(&ptAgeList, pt);
      agelist_addBack(&ptAgeList, pt);
    }
  }

  spinlock_release(shi);
}

static void
depend_entry_invalidate_impl(const DependEntry *entry, int slot)
{
  Mapping *map = entry->map;
  size_t mask = entry->slotMask;

  if (UsingPAE) {
    uintptr_t offset = (map->pa & COYOTOS_PAGE_ADDR_MASK);
    char *map_base = TRANSMAP_MAP(map->pa - offset, char *);
    IA32_PAE *pte = (IA32_PAE *)(map_base + offset);
    size_t maxpte = COYOTOS_PAGE_SIZE / sizeof (*pte);
    
    size_t base = entry->basePTE;
    size_t slotSize = (1u << entry->l2slotSpan);
    size_t slotBias = entry->slotBias;
    assert((base & (slotSize - 1)) == 0);

    for (size_t i = slotBias; i < NUM_GPT_SLOTS; i++) {
      if (slot >= 0 && i != slot)
	continue;
      if (mask & (1u << i)) {
	int biased = i - slotBias;
	assert(base + biased * slotSize + (slotSize - 1) < maxpte);
	for (size_t j = 0; j < slotSize; j++) {
	  size_t slot = base + biased * slotSize + j;
	  if (slot >= map->userSlots)
	    continue;

	  pte_invalidate((struct PTE *)&pte[slot]);
	}
      }
    }
    TRANSMAP_UNMAP(map_base);
  }
  else {
    IA32_PTE *pte = TRANSMAP_MAP(map->pa, IA32_PTE *);
    size_t maxpte = COYOTOS_PAGE_SIZE / sizeof (*pte);
    
    size_t base = entry->basePTE;
    size_t slotSize = (1u << entry->l2slotSpan);
    size_t slotBias = entry->slotBias;
    assert((base & (slotSize - 1)) == 0);

    for (size_t i = slotBias; i < NUM_GPT_SLOTS; i++) {
      if (slot >= 0 && i != slot)
	continue;
      if (mask & (1u << i)) {
	size_t biased = i - slotBias;
	assert(base + biased * slotSize + (slotSize - 1) < maxpte);
	for (size_t j = 0; j < slotSize; j++) {
	  size_t slot = base + biased * slotSize + j;
	  if (slot >= map->userSlots)
	    continue;

	  pte_invalidate((struct PTE *)&pte[slot]);
	}
      }
    }
    TRANSMAP_UNMAP(pte);
  }

  /* Now that all of the entries are invalidated, flush the TLB */
  global_tlb_flush();

  /** @bug need to cross-call other CPUs to have them flush their TLBs.
   * Also, do we want to delay doing this until all of the Depend entries
   * are gone?
   */
}

void
depend_entry_invalidate(const DependEntry *entry)
{
  depend_entry_invalidate_impl(entry, -1);
}

void
depend_entry_invalidate_slot(const DependEntry *entry, size_t slot)
{
  assert (slot >= 0 && slot < NUM_GPT_SLOTS);
  depend_entry_invalidate_impl(entry, slot);
}

void
rm_whack_pte(struct Mapping *map,  size_t slot)
{
  if (UsingPAE) {
    size_t offset = (map->pa & COYOTOS_PAGE_ADDR_MASK);

    char *base = TRANSMAP_MAP(map->pa - offset, char *);

    IA32_PAE *pte = (IA32_PAE *)(base + offset);
    size_t maxpte = COYOTOS_PAGE_SIZE / sizeof (*pte);

    assert(slot < maxpte);
    assert(slot < map->userSlots);
    pte_invalidate((struct PTE *)&pte[slot]);

    TRANSMAP_UNMAP(base);
  } else {
    IA32_PTE *pte = TRANSMAP_MAP(map->pa, IA32_PTE *);
    size_t maxpte = COYOTOS_PAGE_SIZE / sizeof (*pte);

    assert(slot < maxpte);
    pte_invalidate((struct PTE *)&pte[slot]);

    TRANSMAP_UNMAP(pte);
  }

  /* Now that all of the entries are invalidated, flush the TLB */
  global_tlb_flush();

  /** @bug need to cross-call other CPUs to have them flush their TLBs.
   * Also, do we want to delay doing this until all of the revmap entries
   * are gone?
   */
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

#define DEBUGGER_SUPPORT
#ifdef DEBUGGER_SUPPORT
#define MAX_INDENT 40

static void
hwmap_print_indent(int indent)
{
  char spaces[MAX_INDENT + 1];
  int idx;
  for (idx = 0; idx < min(indent, MAX_INDENT); idx++)
    spaces[idx] = ' ';
  spaces[idx] = 0;
  printf("%s", spaces);
}

void
hwmap_dump_pte(uint32_t slot, IA32_PTE *pte, int indent)
{
  if (!pte->bits.SW2) {
    // not soft-valid;  nothing to print
    return;
  }
  hwmap_print_indent(indent);

  printf("0x%03x: fr:0x%llx %s%s%s%s%s%s%s%s%s%s%s%s\n",
	 slot,
	 (kpa_t)PTE_FRAME_TO_KPA(pte->bits.frameno),
	 pte->bits.V ? "VA " : "va ",
	 pte->bits.W ? "WR " : "wr ",
	 pte->bits.USER ? "USR " : "usr ",
	 pte->bits.PWT ? "WT " : "",
	 pte->bits.PCD ? "CD " : "",
	 pte->bits.ACC ? "ACC " : "acc ",
	 pte->bits.DIRTY ? "DTY " : "dty ",
	 pte->bits.PGSZ ? "LGPG " : "",
	 pte->bits.GLBL ? "GLBL " : "",
	 pte->bits.SW0 ? "SW0 " : "sw0 ",
	 pte->bits.SW1 ? "SW1 " : "sw1 ",
	 pte->bits.SW2 ? "SW2 " : "sw2 ");
}

void
hwmap_dump_pae(uint32_t slot, IA32_PAE *pte, int indent)
{
  if (!pte->bits.SW2) {
    // not soft-valid;  nothing to print
    return;
  }
  hwmap_print_indent(indent);

  printf("0x%03x: PA:0x%llx %s%s%s%s%s%s%s%s%s%s%s%s%s\n",
	 slot,
	 (kpa_t)PAE_FRAME_TO_KPA(pte->bits.frameno),
	 pte->bits.V ? "VA " : "va ",
	 pte->bits.W ? "WR " : "wr ",
	 pte->bits.USER ? "USR " : "usr ",
	 pte->bits.PWT ? "WT " : "",
	 pte->bits.PCD ? "CD " : "",
	 pte->bits.ACC ? "ACC " : "acc ",
	 pte->bits.DIRTY ? "DTY " : "dty ",
	 pte->bits.PGSZ ? "LGPG " : "",
	 pte->bits.GLBL ? "GLBL " : "glbl ",
	 pte->bits.SW0 ? "SW0 " : "sw0 ",
	 pte->bits.SW1 ? "SW1 " : "sw1 ",
	 pte->bits.SW2 ? "SW2 " : "sw2 ",
	 pte->bits.NX ? "NX " : (NXSupported ? "nx " : ""));
}

void
hwmap_dump_table(kpa_t kpa, int level, int indent)
{
  hwmap_print_indent(indent);
  printf("PageTable: kpa 0x%08llx, level %d\n", kpa, level);
  if (UsingPAE) {
    if (level == 2) {
      uintptr_t offset = kpa & COYOTOS_PAGE_ADDR_MASK;
      
      uintptr_t tbl = TRANSMAP_MAP(kpa - offset, uintptr_t);
      IA32_PAE *table = (IA32_PAE *)(tbl + offset);
      size_t idx;
      
      for (idx = 0; idx < PDPT_SIZE; idx++) {
	hwmap_dump_pae(idx, &table[idx], indent + 2);
	// if statement is a hack to avoid printing the kernel map
	if (table[idx].bits.V && idx < PDPT_SIZE - 1)
	  hwmap_dump_table(PAE_FRAME_TO_KPA(table[idx].bits.frameno),
			   level - 1, indent + 4);
      }
      TRANSMAP_UNMAP(tbl);
      return;
    }
      
    IA32_PAE *table = TRANSMAP_MAP(kpa, IA32_PAE *);
    size_t idx;

    for (idx = 0; idx < NPAE_PER_PAGE; idx++) {
      hwmap_dump_pae(idx, &table[idx], indent + 2);
      if (level == 1 && table[idx].bits.V)
	hwmap_dump_table(PAE_FRAME_TO_KPA(table[idx].bits.frameno),
			 level - 1, indent + 4);
    }

    TRANSMAP_UNMAP(table);
  } else {
    IA32_PTE *table = TRANSMAP_MAP(kpa, IA32_PTE *);
    size_t idx;

    for (idx = 0; idx < NPTE_PER_PAGE; idx++) {
      hwmap_dump_pte(idx, &table[idx], indent + 2);
      if (level == 1 && table[idx].bits.V)
	hwmap_dump_table(PTE_FRAME_TO_KPA(table[idx].bits.frameno),
			 level - 1, indent + 4);
    }
    TRANSMAP_UNMAP(table);
  }
}
#endif /* DEBUGGER_SUPPORT */
