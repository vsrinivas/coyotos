#ifndef __KERNINC_REVMAP_H__
#define __KERNINC_REVMAP_H__
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
 * @brief Reverse mapping table. */

#include <hal/kerntypes.h>
#include <kerninc/mutex.h>
#include <stddef.h>

struct Mapping;
struct Page;
struct Process;

/** @brief Reverse hardware map.
 *
 * This data structure is used to locate in-pointing mapping table
 * pointers (PTEs) when invalidating a Mapping structure or a page. It
 * compensates for the absence of the KeyKOS/EROS keychain, which
 * would have allowed us to use the depend table for this purpose.
 *
 * This data structure appears in the reverse map hash table, and is
 * indexed by the ObjectHeader address. If a PTE points to an object,
 * there is a corresponding (ObHdr *, pte *) pair in the reverse map.
 */
struct RevMapEntry {
  /** Pointer to either a Mapping structure or an ObjectHeader
   * structure. We do not care which, because we will never indirect
   * through this pointer. We will use it only for identity
   * comparison. The pointed-to structure is either the Mapping or the
   * Page object named by the pointed-to PTE. 
   *
   * We steal the low two bits of the first union for tag bits.
   *
   * JWA and Shap agree that neither "beautiful" nor "elegant"
   * adequately describes this data structure.
   */
  union {
    struct Page    *pPage;	/* LS Bits == 00
				 *
				 * second union is kpa_t, storing
				 * address of pte to whack. */

    struct Mapping *pMap;	/* LS Bits == 01
				 *
				 * second union is pte, storing
				 * address of pte to whack.
				 *
				 * LS Bits == 10
				 *
				 * second union is Process *, storing
				 * virtual address of Process
				 * structure containing the Mapping
				 * pointer.
				 */
    uintptr_t	   raw;
  } target;

  union {
    struct {
      struct Mapping *tbl;
      size_t	     slot;
    } pte;		     /* table and slot for PTE */
    struct Process *proc_va; /* virtual address of Process structure */
  } whackee;
};
typedef struct RevMapEntry RevMapEntry;

#define REVMAP_TARGET_TYPE_MASK (uintptr_t)3
#define REVMAP_TARGET_PTR_MASK	(~REVMAP_TARGET_TYPE_MASK)
#define REVMAP_TARGET_PAGE	0
#define REVMAP_TARGET_MAP_PTE	1
#define REVMAP_TARGET_MAP_PROC	2

enum { ENTRIES_PER_REVMAP = 15 };

struct RevMap {
  struct RevMap *next;  /**< @brief Next RevMap in chain */
  size_t	nvalid; /**< @brief number of valid entries */
  RevMapEntry	ents[ENTRIES_PER_REVMAP]; /**< @brief entries */
};
typedef struct RevMap RevMap;

/** @brief Record a Process's top mapping pointer
 *
 * Precondition: The PTE has a canary value
 *
 * Postcondition: The { Mapping, Process } pair is in the revmap.
 */
void rm_install_process_mapping(struct Mapping *map, struct Process *proc);

/** @brief Record a PTE pointing to a mapping into the revmap 
 *
 * Postcondition: The { Mapping, PTE } pair has been entered into the revmap.
 */
void rm_install_pte_mapping(struct Mapping *map, 
			    struct Mapping *tbl, size_t slot);

/** @brief Install a PTE pointing to a page into the revmap 
 *
 * Precondition: The PTE has a canary value
 *
 * Postcondition: The { Page, PTE } pair has been entered into the revmap.
 */
void rm_install_pte_page(struct Page *page,
			 struct Mapping *tbl, size_t slot);

/** @brief Whack all of the revmap entries for a Mapping */
void rm_whack_mapping(struct Mapping *);
/** @brief Whack all of the revmap entries for a Page */
void rm_whack_page(struct Page *);

/** @brief Whack the specified PTE. */
__hal void rm_whack_pte(struct Mapping *, size_t slot);
/** @brief Whack the specified Process top Mapping pointer. */
__hal void rm_whack_process(struct Process *);

#endif /* __KERNINC_REVMAP_H__ */
