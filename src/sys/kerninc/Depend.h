#ifndef __KERNINC_DEPEND_H__
#define __KERNINC_DEPEND_H__
/*
 * Copyright (C) 2006, Jonathan S. Shapiro.
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
 * @brief Definition of Wrapper structure. */

#include <obstore/GPT.h>
#include <kerninc/ccs.h>
#include <kerninc/capability.h>
#include <kerninc/ObjectHeader.h>
#include <hal/config.h>
#include <hal/vm.h>

/** @brief The depend structure.
 *
 * The Depend structure tracks the relationship from GPT structures
 * to the hardware page table entries that are produced from
 * them. This provides the information that we need to correctly
 * invalidate the hardware entries when a slot in a GPT is
 * overwritten.
 *
 * This data structure appears in the depend table, and is indexed by
 * the MemHeader address.
 */
struct DependEntry {
  /** @brief The key for the hash;  the GPT effected.  
   *
   * In free DependEntrys, this is NULL.
   */
  struct GPT *gpt;
  struct Mapping *map;

#if HIERARCHICAL_MAP
  /** @brief slots which may have entries.
   *
   * If bit N is set, then the 2^l2slotSpan PTEs at:
   *
   *   @p basePTE + (N - @p slotBias) @<@< l2slotSpan
   *
   * Are implicated by this depend entry.
   */
  uint32_t   slotMask : NUM_GPT_SLOTS;
  uint32_t   slotBias : GPT_SLOT_INDEX_BITS;
  uint32_t   l2slotSpan : 6;
  uint32_t   basePTE : HIERARCHICAL_MAP_MAX_PTE_BITS;
#else
#error DependEntry need update for non-hierarchical map.
#endif
};
typedef struct DependEntry DependEntry;

enum { ENTRIES_PER_DEPEND = 15 };

struct Depend {
  struct Depend *next;	/**< @brief Next depend entry in chain */
  size_t	nvalid; /**< @brief number of valid entries */
  DependEntry	ents[ENTRIES_PER_DEPEND]; /**< @brief entries */
};
typedef struct Depend Depend;

/**
 * @brief Install a requested depend entry in the depend table, merging it
 * with other entries if possible.
 *
 * Preconditions: The GPT referenced in @p toInstall must be locked.
 *
 * Postcondition: The requested depend entry is in the depend table.
 */
void depend_install(DependEntry toInstall);

/**
 * @brief Invalidate all depend entries associated with the GPT @p gpt.
 */
void depend_invalidate(struct GPT *gpt);

/**
 * @brief Invalidate all depend entries associated with slot @p slot in 
 * the GPT @p gpt.
 */
void depend_invalidate_slot(struct GPT *gpt, size_t slot);

/**
 * @brief HAL function to invalidate all Mapping entries associated with
 * a DependEntry.
 */
__hal void depend_entry_invalidate(const DependEntry *entry);

/**
 * @brief HAL function to invalidate all Mapping entries associated with
 * a particular slot in a DependEntry.
 */
__hal void depend_entry_invalidate_slot(const DependEntry *entry, size_t slot);

#endif /* __KERNINC_DEPEND_H__ */
