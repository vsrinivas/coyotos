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
 * @brief Reverse map management.
 */

#include <kerninc/RevMap.h>
#include <kerninc/Cache.h>
#include <kerninc/assert.h>
#include <kerninc/Process.h>
#include <kerninc/Mapping.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/printf.h>

#define REVMAP_TABLE_SIZE 1024

typedef struct RevMapBucket {
  spinlock_t lock;
  RevMap *list;
} RevMapBucket;

RevMapBucket revMapTable[REVMAP_TABLE_SIZE];

static inline RevMapBucket *
rm_hash_page(Page *pg)
{
  size_t bucket = ((uintptr_t)pg / sizeof (*pg)) % REVMAP_TABLE_SIZE;

  return (&revMapTable[bucket]);
}

static inline RevMapBucket *
rm_hash_mapping(Mapping *map)
{
  size_t bucket = ((uintptr_t)map / sizeof (*map)) % REVMAP_TABLE_SIZE;

  return (&revMapTable[bucket]);
}

static inline bool
rm_match_page(RevMapEntry ent, Page *pg)
{
  return (ent.target.raw & REVMAP_TARGET_PTR_MASK) == (uintptr_t)pg;
}

static inline bool
rm_match_mapping(RevMapEntry ent, Mapping *map)
{
  return (ent.target.raw & REVMAP_TARGET_PTR_MASK) == (uintptr_t)map;
}

/** @brief Underlying implementation for all of the rm_install_*() routines.
 */
static inline void
rm_install_entry(RevMapBucket *b, RevMapEntry e)
{
  SpinHoldInfo shi = spinlock_grab(&b->lock);

  RevMap *ent;
  bool free = false;

  for (ent = b->list; ent != NULL; ent = ent->next) {
    if (ent->nvalid < ENTRIES_PER_REVMAP)
      free = true;
    for (int x = 0; x < ENTRIES_PER_REVMAP; x++) {
      if (ent->ents[x].target.raw == e.target.raw) {
	switch (e.target.raw & REVMAP_TARGET_TYPE_MASK) {
	case REVMAP_TARGET_PAGE:
	case REVMAP_TARGET_MAP_PTE:
	  if (ent->ents[x].whackee.pte.tbl == e.whackee.pte.tbl &&
	      ent->ents[x].whackee.pte.slot == e.whackee.pte.slot) {
	    spinlock_release(shi);
	    return;
	  }
	  break;

	case REVMAP_TARGET_MAP_PROC:
	  if (ent->ents[x].whackee.proc_va == e.whackee.proc_va) {
	    spinlock_release(shi);
	    return;
	  }
	  break;

	default:
	  assert(0);
	  break;
	}
      }
    }
  }

  if (!free) {
    RevMap *nRev = cache_alloc_RevMap();
    /** @bug we need to invalidate revmap entries if this fails */
    assert(nRev != NULL);
    nRev->next = b->list;
    b->list = nRev;
  }

  for (ent = b->list; ent != NULL; ent = ent->next) {
    if (ent->nvalid >= ENTRIES_PER_REVMAP)
      continue;
    for (size_t i = 0; i < ENTRIES_PER_REVMAP; i++) {
      if (ent->ents[i].target.raw == 0) {
	ent->ents[i] = e;
	ent->nvalid++;
	assert(ent->nvalid <= ENTRIES_PER_REVMAP);
	spinlock_release(shi);
	return;
      }
    }
  }
  /* this should never be reached */
  fatal("There should have been at least one free entry in revmap bucket");

}

void
rm_install_process_mapping(Mapping *map, Process *proc)
{
  RevMapEntry e;
  e.target.raw = (uintptr_t)map | REVMAP_TARGET_MAP_PROC;
  e.whackee.proc_va = proc;

  rm_install_entry(rm_hash_mapping(map), e);
}

void
rm_install_pte_mapping(Mapping *map, Mapping *tbl, size_t slot)
{
  RevMapEntry e;
  e.target.raw = (uintptr_t)map | REVMAP_TARGET_MAP_PTE;
  e.whackee.pte.tbl = tbl;
  e.whackee.pte.slot = slot;

  rm_install_entry(rm_hash_mapping(map), e);
}

void
rm_install_pte_page(Page *pg, Mapping *tbl, size_t slot)
{
  RevMapEntry e;
  e.target.raw = (uintptr_t)pg | REVMAP_TARGET_PAGE;
  e.whackee.pte.tbl = tbl;
  e.whackee.pte.slot = slot;

  rm_install_entry(rm_hash_page(pg), e);
}

static inline void
rm_do_whack(RevMapEntry e)
{
  switch (e.target.raw & REVMAP_TARGET_TYPE_MASK) {
  case REVMAP_TARGET_PAGE:
  case REVMAP_TARGET_MAP_PTE:
    rm_whack_pte(e.whackee.pte.tbl, e.whackee.pte.slot);
    break;
  case REVMAP_TARGET_MAP_PROC:
    rm_whack_process(e.whackee.proc_va);
    break;
  default:
    assert(0);
    break;
  }
}

void 
rm_whack_mapping(struct Mapping *m)
{
  RevMapBucket *b = rm_hash_mapping(m);

  SpinHoldInfo shi = spinlock_grab(&b->lock);

  RevMap *ent;

  for (ent = b->list; ent != NULL; ent = ent->next) {
    for (int x = 0; x < ENTRIES_PER_REVMAP; x++) {
      if (rm_match_mapping(ent->ents[x], m)) {
	rm_do_whack(ent->ents[x]);
	ent->ents[x].target.raw = 0;
	ent->ents[x].whackee.pte.tbl = 0;
	ent->ents[x].whackee.pte.slot = 0;
	assert(ent->nvalid > 0);
	ent->nvalid--;
      }
    }
  }
  spinlock_release(shi);
}

void rm_whack_page(struct Page *pg)
{
  RevMapBucket *b = rm_hash_page(pg);

  SpinHoldInfo shi = spinlock_grab(&b->lock);

  RevMap *ent;

  for (ent = b->list; ent != NULL; ent = ent->next) {
    for (int x = 0; x < ENTRIES_PER_REVMAP; x++) {
      if (rm_match_page(ent->ents[x], pg)) {
	rm_do_whack(ent->ents[x]);
	ent->ents[x].target.raw = 0;
	ent->ents[x].whackee.pte.tbl = 0;
	ent->ents[x].whackee.pte.slot = 0;
	assert(ent->nvalid > 0);
	ent->nvalid--;
      }
    }
  }
  spinlock_release(shi);
}
