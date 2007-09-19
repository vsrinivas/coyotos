#ifndef HAL_CONFIG_H
#define HAL_CONFIG_H
/*
 * Copyright (C) 2005, The EROS Group, LLC.
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
 * @brief Definition of miscellaneous configuration paramters.
 */

#include <target-hal/config.h>

/** @brief Maximum number of CPUs that will be used.
 *
 * Must be a power of two.
 */
#ifndef MAX_NCPU
#define MAX_NCPU 1
#endif

#ifndef HAVE_HUI
/** @brief Whether we have a human user interface. If not, there is no
 * point compiling strings and printf calls into the kernel.
 */
#define HAVE_HUI 1
#endif

#ifndef HAVE_CONSOLE
/** @brief Whether we have a console.
 */
#define HAVE_CONSOLE 0
#endif

#ifndef HIERARCHICAL_MAP
/** @brief Set to 1 if target uses a hierarchical mapping scheme.
 *
 * Should be zero for hashed mapping schemes or non-hierarchical
 * software translation schemes */
#define HIERARCHICAL_MAP 0
#endif

#ifndef PHYSMEM_NREGIONS
/** @brief Number of physical memory region descriptors to allocate.
 * This is a reasonable, but probably not excessive, default.
 */
#define PHYSMEM_NREGIONS 64
#endif /* PHYSMEM_NREGIONS */

#ifndef CACHE_LINE_SIZE
/** @brief Approximate size of a cache line. 
 *
 * This is really only a hint, and it is okay for it not to be quite right.
 */
#define CACHE_LINE_SIZE 32
#endif /* CACHE_LINE_SIZE */

#ifndef KSTACK_NPAGES
/** @brief Number of pages in each per-CPU stack.
 *
 * This value <em>must</em> be a power of two, and the per-CPU stack
 * <em>must</em> be aligned at a boundary that is the same as its
 * size. This is necessary in order for the curCPU macro to work. 
 */
#define KSTACK_NPAGES   0x1
#endif /* KSTACK_NPAGES */

#ifndef MAPTABLES_PER_PROCESS
/** @brief Number of top-level mapping pointers in each process.
 *
 * On some architectures, this may be greater than one.
 */
#define MAPTABLES_PER_PROCESS   0x1
#endif /* MAPTABLES_PER_PROCESS */

/** @brief Size of per-CPU kernel stack in bytes. */
#define KSTACK_SIZE     (KSTACK_NPAGES * COYOTOS_PAGE_SIZE)

/** @brief Mask value used to strip stack offset from stack pointer
    value. */
#define KSTACK_MASK     (~(KSTACK_NPAGES * COYOTOS_PAGE_SIZE -1))

#endif /* HAL_CONFIG_H */
