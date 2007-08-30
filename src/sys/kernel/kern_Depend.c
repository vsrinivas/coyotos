/*
 * Copyright (C) 2007, The EROS Group, LLC.
 *
 * This file is part of the EROS Operating System.
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
 * @brief Depend Table management
 */

#include <kerninc/Depend.h>
#include <kerninc/Mapping.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/GPT.h>
#include <kerninc/Cache.h>
#include <kerninc/assert.h>
#include <kerninc/printf.h>
#include <hal/machine.h>

#define DEPEND_TABLE_SIZE 1024

typedef struct DependBucket {
  spinlock_t lock;
  Depend *list;
} DependBucket;

DependBucket dependTable[DEPEND_TABLE_SIZE];

static inline DependBucket *
depend_hash(GPT *gpt)
{
  size_t bucket = ((uintptr_t)gpt / sizeof (*gpt)) % DEPEND_TABLE_SIZE;

  return (&dependTable[bucket]);
}

/**
 * @brief Attempt to merge the new depend entry @p n with an existing 
 * depend entry @p e.
 *
 * If the entries are not mergable, returns false without modifying @p e.
 *
 * If the entries are mergable, merges the data from @p n into @p e
 * and returns true.
 */
static inline bool
depend_merge(DependEntry *e, DependEntry n)
{
  if (e->gpt != n.gpt ||
      e->map != n.map)
    return false;

#if HIERARCHICAL_MAP
  /* to be mergable, the l2slotSpans must match, and the implied base PTE
   * must be identical.
   */
  if (e->l2slotSpan != n.l2slotSpan ||
      (e->basePTE - (e->slotBias << e->l2slotSpan)) !=
      (n.basePTE - (n.slotBias << n.l2slotSpan)))
    return false;
  
  if (n.slotBias < e->slotBias)
    e->slotBias = n.slotBias;
  if (n.basePTE < e->basePTE)
    e->basePTE = n.basePTE;
  e->slotMask |= n.slotMask;
#else
#error Only Hierarchical map is implemented.
#endif
  return true;
}

void 
depend_install(DependEntry arg)
{
  assert(arg.gpt != NULL);

  DependBucket *b = depend_hash(arg.gpt);
  SpinHoldInfo hi = spinlock_grab(&b->lock);
  Depend *cur;
  bool free = false;

  for (cur = b->list; cur; cur = cur->next) {
    if (cur->nvalid < ENTRIES_PER_DEPEND)
      free = true;
    for (size_t i = 0; i < ENTRIES_PER_DEPEND; i++) {
      if (depend_merge(&cur->ents[i], arg)) {
	spinlock_release(hi);
	return;
      }
    }
  }
  if (!free) {
    Depend *nDep = cache_alloc_Depend();
    /** @bug we need to invalidate depend entries if this fails */
    assert(nDep != NULL);
    nDep->next = b->list;
    b->list = nDep;
  }
  for (cur = b->list; cur; cur = cur->next) {
    if (cur->nvalid >= ENTRIES_PER_DEPEND)
      continue;
    for (size_t i = 0; i < ENTRIES_PER_DEPEND; i++) {
      if (cur->ents[i].gpt == 0) {
	cur->ents[i] = arg;
	cur->nvalid++;
	assert(cur->nvalid <= ENTRIES_PER_DEPEND);
	spinlock_release(hi);
	return;
      }
    }
  }
  /* this should never be reached */
  fatal("There should have been at least one free entry in depend bucket");
}

void
depend_invalidate(GPT *gpt)
{
  DependBucket *b = depend_hash(gpt);
  SpinHoldInfo hi = spinlock_grab(&b->lock);
  Depend *cur;

  for (cur = b->list; cur; cur = cur->next) {
    for (size_t i = 0; i < ENTRIES_PER_DEPEND; i++) {
      if (cur->ents[i].gpt == gpt) {
	depend_entry_invalidate(&cur->ents[i]);
	cur->ents[i].gpt = 0;
	assert(cur->nvalid > 0);
	cur->nvalid--;
      }
    }
  }
  spinlock_release(hi);
}

void
depend_invalidate_slot(GPT *gpt, size_t slot)
{
  DependBucket *b = depend_hash(gpt);
  SpinHoldInfo hi = spinlock_grab(&b->lock);
  Depend *cur;
#if HIERARCHICAL_MAP
  size_t mask = 1u << slot;
#else
#error non-hierachical maps are not implemented
#endif

  for (cur = b->list; cur; cur = cur->next) {
    for (size_t i = 0; i < ENTRIES_PER_DEPEND; i++) {
      if (cur->ents[i].gpt == gpt) {
	if (!(cur->ents[i].slotMask & mask))
	  continue;

	depend_entry_invalidate_slot(&cur->ents[i], slot);
	cur->ents[i].slotMask &= ~mask;

	if (cur->ents[i].slotMask == 0) {
	  cur->ents[i].gpt = 0;
	  assert(cur->nvalid > 0);
	  cur->nvalid--;
	}
      }
    }
  }
  spinlock_release(hi);
}
