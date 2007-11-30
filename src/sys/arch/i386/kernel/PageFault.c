/*
 * Copyright (C) 2007, The EROS Group, LLC
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
 * @brief Page Fault implementation.
 */

#include <stddef.h>
#include <kerninc/assert.h>
#include <kerninc/printf.h>
#include <kerninc/PhysMem.h>
#include <kerninc/Cache.h>
#include <kerninc/MemWalk.h>
#include <kerninc/GPT.h>
#include <kerninc/util.h>
#include <kerninc/Process.h>
#include <kerninc/Mapping.h>
#include <kerninc/Depend.h>
#include <kerninc/pstring.h>
#include "hwmap.h"
#include "kva.h"

#define DEBUG_PGFLT if (0)
#define DEBUG_TRANSMAP if (0)

/** @brief A description of a level of PageTables. */
typedef const struct PageTableLevel {
  /** @brief log_2 of the space covered by a table. */
  uint8_t l2table;

  /** @brief Page Table Level (used in pgtbl_get()) */
  uint8_t level;

  /** @brief log_2 of the space covered by a slot, or zero if this
   * level describes pages. 
   */
  uint8_t l2slot;
  /**< @brief hardware-supported restr in table slots */
  uint8_t slot_restr;

#ifdef NEED_KPA_TO_MAPPING
  /** @brief routine to convert a KPA @p kpa to a Mapping pointer at this
   * level.
   */
  Mapping *(*KPAToMapping)(kpa_t kpa);
#endif /* NEED_KPA_TO_MAPPING */

  /** @brief Get a kernel VA pointer to a particular PTE, identified
   * by @p table and @p slot.
   * 
   * When finished with the returned PTE, finish_pte() should be
   * called on the returned value.
   */
  pte_t *(*findPTE)(Mapping *table, size_t slot);

  /** @brief routine to read the current value of a PTE 
   *
   * If this returns true, @p pte is valid, and
   * @p content and @p restr are filled with the values from the PTE.
   */
  bool (*readPTE)(pte_t *pte,
		  kpa_t /*OUT*/ *content, uint8_t /*OUT*/ *restr);

  /** 
   * @brief Routine to install a canary value in @p pte, in preperation for
   * later use by installPTE.
   */
  void (*canaryPTE)(pte_t *);

  /** @brief Routine to install a PTE to a Page or Mapping in a table.
   *
   * @p pte is the PTE physical address, returned from find_pte().  @p
   * content is the physical address of the page table or page at the
   * next lower level.  @p restr are the restrictions the PTE should
   * have, and will already be masked by the <tt>slot_restr</tt>.  @p
   * isCapPage is set iff we are installing a PTE to a CapPage.
   */
  bool (*installPTE)(pte_t *pte, kpa_t content, uint8_t restr, bool isCapPage);

  /** @brief Release a PTE value we have been manipulating */
  void (*finishPTE)(pte_t *pte);

} PageTableLevel;

static pte_t *
mapPAE(Mapping *map, size_t slot)
{
  size_t offset = map->pa & COYOTOS_PAGE_ADDR_MASK;
  kpa_t base = map->pa - offset;

  uintptr_t vbase = TRANSMAP_MAP(base, uintptr_t);
  IA32_PAE *ptr = (IA32_PAE *)(vbase + offset);

  DEBUG_TRANSMAP
    printf("xmapped PA 0x%llx slot 0x%lx; va %x;  PAE's value is 0x%llx\n",
	 map->pa, slot, &ptr[slot], ptr[slot].value);
  return ((pte_t *)&ptr[slot]);
}

static void
unmapPAE(pte_t *pte)
{
  DEBUG_TRANSMAP printf("unmapping va 0x%x\n", pte);
  uintptr_t base = ((uintptr_t)pte) & ~COYOTOS_PAGE_ADDR_MASK;
  TRANSMAP_UNMAP((void *)base);
}

static bool
readPAE(pte_t *pte, kpa_t /*OUT*/ *content, uint8_t /*OUT*/ *restr)
{
  IA32_PAE val;
  memcpy(&val, pte, sizeof(val));

  if (!val.bits.SW2)
    return false;   /* not soft-valid, must have been invalidated */

  *content = PAE_FRAME_TO_KPA(val.bits.frameno);
  *restr = 
    (val.bits.W?   0            : CAP_RESTR_RO) |
    (val.bits.SW0? CAP_RESTR_WK : 0) |
    (val.bits.NX?  CAP_RESTR_NX : 0);
  return true;
}

static IA32_PAE PAE_canary = {
  {
    0,				/* Valid */
    1,				/* writable */
    0,				/* user-accessable */
    0,				/* write-through */
    0,				/* cache disable */
    0,				/* accessed */
    0,				/* dirty */
    0,				/* large page */
    0,				/* global */
    0,				/* SW0:  Weak */
    0,				/* SW1: unused */
    0,				/* SW2: soft-valid */
    0,				/* physical frame */
    0,				/* reserved: must be zero */
    0				/* No-execute */
  }
};
  
static void
canaryPAE(pte_t *pte)
{
  IA32_PAE *target = (IA32_PAE *)pte;

  memcpy(target, &PAE_canary, sizeof (*target));
}

static bool
installPAE(pte_t *pte, kpa_t content, uint8_t restr, bool isCapPage)
{
  IA32_PAE *target = (IA32_PAE *)pte;

  IA32_PAE val = {
    {
      (isCapPage) ? 0 : 1,		/* Valid */
      (restr & CAP_RESTR_RO)? 0 : 1,	/* writable */
      1,				/* user-accessable */
      (restr & CAP_RESTR_WT) ? 1 : 0,   /* write-through */
      (restr & CAP_RESTR_CD) ? 1 : 0,   /* cache disable */
      1,				/* accessed */
      1,				/* dirty */
      0,				/* large page */
      0,				/* global */
      (restr & CAP_RESTR_WK)? 1 : 0,    /* SW0:  Weak */
      0,				/* SW1: unused */
      1,				/* SW2: soft-valid */
      PAE_KPA_TO_FRAME(content),
      0,				/* reserved: must be zero */
      (restr & CAP_RESTR_NX)? 1 : 0	/* No-execute */
    }
  };

  DEBUG_PGFLT printf("Installing physaddr: 0x%llx restr: 0x%x capPage: %d in VA %x\n",
	 content, restr, isCapPage, pte);  
  
  /** @bug use CAS */
  if (memcmp(target, &PAE_canary, sizeof (PAE_canary)) != 0)
    return false;

  memcpy(pte, &val, sizeof(val));
  DEBUG_PGFLT printf("succeeded.  New value: 0x%08llx\n", val.value);

  return true;
}

static bool
installPAE_PDPE(pte_t *pte, kpa_t content, uint8_t restr, bool isCapPage)
{
  IA32_PAE *target = (IA32_PAE *)pte;

  IA32_PAE val = {
    {
      (isCapPage) ? 0 : 1,		/* Valid */
      0,				/* MBZ */
      0,				/* MBZ */
      0,				/* write-through */
      0,				/* cache disable */
      0,				/* MBZ */
      0,				/* MBZ */
      0,				/* MBZ */
      0,				/* MBZ */
      (restr & CAP_RESTR_WK)? 1 : 0,    /* SW0:  Weak */
      0,				/* SW1: unused */
      1,				/* SW2: soft-valid */
      PAE_KPA_TO_FRAME(content),
      0,				/* reserved: must be zero */
      (restr & CAP_RESTR_NX)? 1 : 0	/* No-execute */
    }
  };

  DEBUG_PGFLT 
    printf("Installing physaddr: 0x%llx restr: 0x%x capPage: %d in VA %x\n",
	 content, restr, isCapPage, pte);  
  
  /** @bug use CAS */
  if (memcmp(target, &PAE_canary, sizeof (PAE_canary)) != 0)
    return false;

  memcpy(pte, &val, sizeof(val));
  DEBUG_PGFLT printf("succeeded.  New value: 0x%08llx\n", val.value);

  return true;
}

static pte_t *
mapPTE(Mapping *map, size_t slot)
{
  size_t offset = map->pa & COYOTOS_PAGE_ADDR_MASK;
  kpa_t base = map->pa - offset;

  uintptr_t vbase = TRANSMAP_MAP(base, uintptr_t);
  IA32_PTE *ptr = (IA32_PTE *)(vbase + offset);

  return ((pte_t *)&ptr[slot]);
}

static void
unmapPTE(pte_t *pte)
{
  uintptr_t base = ((uintptr_t)pte) & ~COYOTOS_PAGE_ADDR_MASK;
  TRANSMAP_UNMAP((void *)base);
}

static bool
readPTE(pte_t *pte, kpa_t /*OUT*/ *content, uint8_t /*OUT*/ *restr)
{
  IA32_PTE val;
  memcpy(&val, pte, sizeof(val));

  if (!val.bits.SW2)
    return false;   /* not soft-valid, must have been invalidated */

  *content = PTE_FRAME_TO_KPA(val.bits.frameno);
  *restr = 
    (val.bits.W?   0            : CAP_RESTR_RO) |
    (val.bits.SW0? CAP_RESTR_WK : 0);

  return true;
}

static IA32_PTE PTE_canary = {
  {
    0,				/* Valid */
    1,				/* writable */
    0,				/* user-accessable */
    0,				/* write-through */
    0,				/* cache disable */
    0,				/* accessed */
    0,				/* dirty */
    0,				/* large page */
    0,				/* global */
    0,				/* SW0:  Weak */
    0,				/* SW1: unused */
    0,				/* SW2: soft-valid */
    0				/* physical frame */
  }
};

static void
canaryPTE(pte_t *pte)
{
  IA32_PTE *target = (IA32_PTE *)pte;
  
  memcpy(target, &PTE_canary, sizeof (*target));
}


static bool
installPTE(pte_t *pte, kpa_t content, uint8_t restr, bool isCapPage)
{
  IA32_PTE *target = (IA32_PTE *)pte;

  IA32_PTE val = {
    {
      (isCapPage) ? 0 : 1,		/* Valid */
      (restr & CAP_RESTR_RO)? 0 : 1,	/* writable */
      1,				/* user-accessable */
      (restr & CAP_RESTR_WT) ? 1 : 0,   /* write-through */
      (restr & CAP_RESTR_CD) ? 1 : 0,   /* cache disable */
      1,				/* accessed */
      1,				/* dirty */
      0,				/* large page */
      0,				/* global */
      (restr & CAP_RESTR_WK)? 1 : 0,    /* SW0:  Weak */
      0,				/* SW1: unused */
      1,				/* SW2: soft-valid */
      PTE_KPA_TO_FRAME(content)
    }
  };
    
  /** @bug use CAS */
  if (memcmp(target, &PTE_canary, sizeof (PTE_canary)) != 0)
    return false;

  memcpy(pte, &val, sizeof(val));
  return true;
}

const PageTableLevel paePtbl[] = {
  { 32, 2, 30, 0, 				      
    mapPAE, readPAE, canaryPAE, installPAE_PDPE, unmapPAE },
  { 30, 1, 21, 0,
    mapPAE, readPAE, canaryPAE, installPAE, unmapPAE },
  { 21, 0, 12, CAP_RESTR_RO|CAP_RESTR_NX|CAP_RESTR_CD|CAP_RESTR_WT,
    mapPAE, readPAE, canaryPAE, installPAE, unmapPAE },
  { 0 }
};

const PageTableLevel normPtbl[] = {
  { 32, 1, 22, 0,
    mapPTE, readPTE, canaryPTE, installPTE, unmapPTE },
  { 22, 0, 12, CAP_RESTR_RO|CAP_RESTR_CD|CAP_RESTR_WT,
    mapPTE, readPTE, canaryPTE, installPTE, unmapPTE },
  { 0 }
};

void
do_pageFault(Process *base, uintptr_t addr_arg, 
	     bool wantWrite, 
	     bool wantExec,
	     bool wantCap)
{
  uintptr_t addr = addr_arg;

  coyotos_Process_FC result;
  MemWalkResults mwr;
  MemWalkEntry *mwe;
  size_t restr;
  /* minl2 is the minimum l2g or l2v value seen so far. */
  size_t minl2 = COYOTOS_HW_ADDRESS_BITS;
  bool first;

  size_t restr_mask = 
    CAP_RESTR_RO | CAP_RESTR_WK | (IA32_NXSupported ? CAP_RESTR_NX : 0);
  size_t leaf_restr_mask = 
    CAP_RESTR_CD | CAP_RESTR_WT | restr_mask;

  result = memwalk(&base->state.addrSpace, addr, wantWrite, &mwr);

  if (addr >= KVA) {
    result = coyotos_Process_FC_InvalidDataReference;
    goto deliver_fault;
  }

  if (wantExec && (mwr.cum_restr & CAP_RESTR_NX)) {
    result = coyotos_Process_FC_NoExecute;
    goto deliver_fault;
  }

  if (result != 0)
    goto deliver_fault;

  if (!wantCap && (mwr.ents[mwr.count - 1].entry->hdr.ty == ot_CapPage)) {
    result = coyotos_Process_FC_DataAccessTypeError;
    goto deliver_fault;
  }

  mwe = &mwr.ents[0];
  restr = 0;

  first = true;

  /* 
   * First, handle the Process top-level pointer, since it's special.
   * Unlike lower down levels, the target of the addrSpace capability
   * is *always* the producer of the top-level table.  This prevents
   * us from needing Depend entries that target Processes.
   */

  restr |= (mwe->restr & restr_mask);
  minl2 = min(minl2, mwe->l2g);

  const PageTableLevel *curPT = IA32_UsingPAE? paePtbl : normPtbl;

  /* The top-level pointer has no restrictions, so they must all be
     pushed into the mapping table. */
  Mapping *curmap = 
    pgtbl_get(mwe->entry, curPT->level, mwe->guard,
	      ~(((coyaddr_t)2 << (minl2 - 1)) - 1), restr);

  base->mappingTableHdr = 0; /** @bug use canary value */

  rm_install_process_mapping(curmap, base);

  /** @bug use CAS */
  base->mappingTableHdr = curmap;
  
  for (;
       curPT->l2slot != 0;
       curPT++) {
    /** 
     * we've found the next mapping table and installed its pointer.
     * Now, we need to figure out the next slot, fill it with a canary
     * value, and install depend entries for any GPTs we walk through
     * on the way to the next mapping level.
     */
    
    size_t slot = addr >> curPT->l2slot;
    addr -= slot << curPT->l2slot;

    pte_t *pte = curPT->findPTE(curmap, slot);
    
    curPT->canaryPTE(pte);

    // Loop to pass through the non-producing GPTs, so that we can find
    // the next producer.
    for (;;) {
      // stop if this is the producer of the next page table
      if (!mwe->window && 
	  (minl2 < curPT->l2slot || mwe->l2v < curPT->l2slot))
	break;

      if (mwe->slot != MEMWALK_SLOT_BACKGROUND) {
	// generate a depend entry for this GPT
	assert(mwe->entry->hdr.ty == ot_GPT);

	minl2 = min(minl2, mwe->l2v);

	DependEntry de;
	de.gpt = (GPT *)mwe->entry;
	de.map = curmap;
	de.slotMask = 1u << mwe->slot;
	de.slotBias = mwe->slot;
	de.l2slotSpan = min(minl2, curPT->l2table) - curPT->l2slot;
	de.basePTE = slot & ~((1u << de.l2slotSpan) - 1);

	depend_install(de);
      }

      mwe++;
      restr |= (mwe->restr & restr_mask);
      minl2 = min(minl2, mwe->l2g);
      continue;
    }

    kpa_t target_pa;
    Mapping *newmap = 0;

    if (curPT->l2slot == COYOTOS_PAGE_ADDR_BITS) {
      // the page, boss, the page!
      assert(mwe->entry->hdr.ty == ot_Page || 
	     mwe->entry->hdr.ty == ot_CapPage);
      rm_install_pte_page((Page *)mwe->entry, curmap, slot);
      
      // This might be a frame named by a physical page, in which case
      // we need to incorporate the CAP_RESTR_CD and CAP_RESTER_WT
      // bits at this stage. Those bits *only* apply to leaf pages, so
      // we can add them here.
      restr |= (mwe->restr & leaf_restr_mask);

      // To install a writable mapping to a page, it must be marked dirty
      if (!(restr & (CAP_RESTR_RO|CAP_RESTR_WK)) && wantWrite) {
	/* Target page may be immutable. Is this the right result
	   code? */
	if (!obhdr_dirty(&mwe->entry->hdr)) {
	  result = coyotos_Process_FC_AccessViolation;
	  goto deliver_fault;
	}
      } else {
	/* if the page isn't already dirty, make the PTE read-only. */
	if (!mwe->entry->hdr.dirty)
	  restr |= CAP_RESTR_RO;
      }
      target_pa = ((Page *)mwe->entry)->pa;
    } else {
      coyaddr_t guard = mwe->guard;

      // We know that this GPT is producing the next page table.
      if (minl2 > curPT->l2slot) {
	assert(mwe->l2v < curPT->l2slot);
	// This is a split case;  the GPT is producing multiple Page Table 
	// entries.
	guard = (mwe->slot << mwe->l2v) & ~((1 << curPT->l2slot) - 1);
      }
      else {
	/* clear the bits above the table size */
	guard &= ~(((coyaddr_t)1 << curPT->l2slot) - 1);
      }

      /** @bug We need to be holding the mappingListLock until
       * the RevMap entry is installed.
       */
      newmap = pgtbl_get(mwe->entry, curPT->level - 1, guard,
			 ~(((coyaddr_t)2 << (minl2 - 1)) - 1),
			 (restr & ~curPT->slot_restr));

      rm_install_pte_mapping(newmap, curmap, slot);

      target_pa = newmap->pa;
    }
    /* attempt to install the PTE; if this fails, one of our depend entries
     * might have been whacked.  We must start over from scratch.
     */
    if (!curPT->installPTE(pte,
			   target_pa, 
			   (restr & curPT->slot_restr),
			   (mwe->entry->hdr.ty == ot_CapPage))) {
      curPT->finishPTE(pte);
      sched_restart_transaction();
    }
    curPT->finishPTE(pte);

    /* Clear the restrictions the pointer contains. */
    restr &= ~curPT->slot_restr;
    curmap = newmap;
  }

  /** @bug Why is this necessary? */
  local_tlb_flushva(addr_arg);

  return;

 deliver_fault:
  proc_deliver_memory_fault(MY_CPU(current), result, addr, &mwr);
  /*  proc_TakeFault(MY_CPU(current), result, addr); */
}
