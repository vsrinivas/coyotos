#ifndef __KERNINC_MEMWALK_H__
#define __KERNINC_MEMWALK_H__
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
 *
 * @brief Memory tree walkers
 */

#include <stddef.h>
#include <coyotos/coytypes.h>
#include <kerninc/capability.h>
#include <kerninc/ObjectHeader.h>
#include <idl/coyotos/Process.h>

/** @brief Maximum number of Memory caps in walk */
#define MEMWALK_MAX  32     

#define MEMWALK_SLOT_BACKGROUND 0xff
typedef struct MemWalkEntry {
  /** @brief guard of capability (after left shift by l2g) */
  coyaddr_t guard;

  /** @brief remaining address (after guard has been removed) */
  coyaddr_t remAddr;

  /** @brief The memory header of the capability traversed.
   *
   * In the case of a window capability, this will point to the
   * containing the @em target GPT of the window.
   */
  MemHeader *entry;

  /** @brief l2g from capability 
   *
   * For GPTs, this is actually min(l2g from cap, l2v + slot_width)) */
  uint8_t l2g;

  /**< @brief restrictions from capability */
  uint8_t restr;

  /** @brief slot in GPT accessed.
   *
   * 0 if not a GPT or window capability.
   * MEMWALK_SLOT_BACKGROUND for a background window capability.
   * targetted slot for a local window capability.
   */
  uint8_t slot;	 

  /** @brief l2v of GPT accessed, or 0 for pages, or same as l2g otherwise */
  uint8_t l2v;

  /** @brief access was through a window capability
   *
   * Does not use bool because stdbool does not specify size of that
   * type, and we want this small.
   */
  uint8_t window;
} MemWalkEntry;

typedef struct MemWalkResults {
  /** @brief Number of valid entries in ents.
   *
   * Will always be @<= MEMWALK_MAX. if memwalk() returns 0,
   * @p count will be @> 0.
   */
  size_t count;
  /** @brief Cumulative restrictions from the walk.
   *
   * This is just the OR of all @p restr fields from the valid @p ents array,
   * collected here because most callers need to know them.
   */
  uint8_t cum_restr;
  /** @brief Array of MemWalkEntrys describing the results of the walk.  
   * 
   * Only the first @p count entries are valid.  Each describes a
   * single capability traversal in the walk, and the order of entries is
   * the same as the order of traversal.
   *
   * 
   */
  MemWalkEntry ents[MEMWALK_MAX];
} MemWalkResults;

/** l2stop value to walk the memory tree to a leaf Page or CapPage */
#define EXTENDED_MEMWALK_L2STOP_TO_PAGE 0

/**
 * @brief Walk the memory tree rooted at @p base for offset @p addr, stopping
 * at the first object smaller than @p l2stop. 
 * 
 * The results are written into @p results.  If @p forWrite is true,
 * we preferentially fail with coyotos_process_FC_AccessViolation if the path
 * is RO or WK.
 */
extern coyotos_Process_FC memwalk(capability *base, 
				  coyaddr_t addr,
				  bool forWrite,
				  MemWalkResults *results /* OUT */);

/**
 * @brief Variant of memwalk that knows how to stop early. 
 * 
 * This calls memwalk() and then conditionally backs out to satisfy
 * the l2stop objective.
 */
extern coyotos_Process_FC extended_memwalk(capability *base, 
					   coyaddr_t addr, uint8_t l2stop,
					   bool forWrite,
					   MemWalkResults *results /* OUT */);

#endif /* __KERNINC_MEMWALK_H__ */
