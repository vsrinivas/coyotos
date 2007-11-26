#ifndef __KERNINC_MAPPING_H__
#define __KERNINC_MAPPING_H__
/*
 * Copyright (C) 2007, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Address mapping management structure.
 */

#include <stdint.h>
#include <kerninc/Link.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/mutex.h>
#include <hal/kerntypes.h>

/** @brief Allocate page table frames and management structures. */
__hal void pagetable_init(void);

/** @brief Abstraction of a virtual address mapping context.
 *
 * Every architecture we know about -- even those that are software
 * translated -- has @em some mapping structure. At a minimum, it is
 * necessary to provide an address space context value that
 * distinguishes one address space from another. The Mapping structure
 * encapsulates this notion.
 *
 * Much of the @em content of the Mapping structure is HAL-dependent.
 * On architectures that use hardware-defined page table structures,
 * the Mapping structure serves as the management structure for the
 * actual page tables.
 */
typedef struct Mapping {
  /** @brief Links in object history list
   *
   * MUST BE FIRST!! 
   */
  Link     ageLink;

  /** @brief Next mapping structure on same product chain. 
   *
   * If chains get long, we may need to consider an alternative
   * linkage structure to facilitate removal. */
  struct Mapping *nextProduct;

  /** @brief Pointer to the object that produced this page table. */
  MemHeader *producer;

#if HIERARCHICAL_MAP
  /** @brief Physical address of this page table */
  kpa_t     pa;

  /** @brief Virtual address bits that must match in order for this
   * table to be appropriate for use.
   */
  coyaddr_t    match;
  /** @brief Virtual address bits that are significant for matching
   * purposes in order for this table to be appropriate for use.
   */
  coyaddr_t    mask;

  /** @brief Permission restrictions that must be applied to all
      entries in this table.
   */
  uint8_t restr;

  /** @brief Level of page table */
  uint8_t level;

  size_t userSlots;  /**< @brief Number of leading user-mode slots */
#else
#error non-Hierarchical map not implemented
#endif /* HIERARCHICAL_MAP */
} Mapping;

/** @brief Flush the TLB on all processors */
__hal void global_tlb_flush();

/** @brief Flush @p va from the TLB on all processors */
__hal void global_tlb_flushva(kva_t va);

__hal extern Mapping KernMapping;

__hal extern spinlock_t mappingListLock;

#endif /* __KERNINC_MAPPING_H__ */
