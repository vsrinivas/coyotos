#ifndef I386_HAL_CONFIG_H
#define I386_HAL_CONFIG_H
/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Definitions of various kernel constants.
 */

#ifndef MAX_NCPU
/** @brief Maximum (static) number of CPUs we will support */
#define MAX_NCPU 4
#endif

/** @brief Whether we have a console.
 */
#define HAVE_CONSOLE 1

/** @brief Number of entries in the physical region list.
 */
#define PHYSMEM_NREGION 512

/** @brief Set to 1 if target uses a hierarchical mapping scheme.
 *
 * Should be zero for hashed mapping schemes or non-hierarchical
 * software translation schemes */
#define HIERARCHICAL_MAP 1

/** @brief Set to the number of bits needed to reference a particular PTE in
 * an arbitrary mapping table.
 *
 * Only referenced in HIERARCHICAL_MAP is 1.
 */
#define HIERARCHICAL_MAP_MAX_PTE_BITS 10

/** @brief Alignment value used for cache aligned data structures.
 *
 * This alignment value cannot be architecture-specific, because on
 * many architectures (e.g. Pentium) it is specific to each
 * implementation. In reality, this is an alignment value chosen to do
 * a ``good enough'' job of reducing cache line misses in practice.
 */
#define CACHE_LINE_SIZE         128

/** @brief Number of top-level mapping tables in each process.
 *
 * On IA-32, we handle the PDPT by pretending that their are four
 * top-level mapping table pointers, each spanning 1Gbyte. This
 * deprives us of the ability to use permissions logic at those
 * levels, but pragmatically that doesn't matter very much.
 */
#define MAPTABLES_PER_PROCESS   4

/** @brief Number of pages in each per-CPU stack.
 *
 * This value <em>must</em> be a power of two, and the per-CPU stack
 * <em>must</em> be aligned at a boundary that is the same as its
 * size. This is necessary in order for the curCPU macro to work. 
 */
#define KSTACK_NPAGES   0x1

/** @brief Starting virtual address for the kernel */
#define KVA 0xC0000000

/** @brief Limit above which heap must not grow.
 *
 * This should be selected to fall *below* any other magic virtual
 * addresses.
 */

#define HEAP_LIMIT_VA            0xFF400000

/** @brief Virtual base address of transient map. MUST be a multiple of 4M */
#define TRANSMAP_WINDOW_KVA 0xFF800000

#endif /* I386_HAL_CONFIG_H */
