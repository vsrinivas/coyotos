#ifndef HAL_VM_H
#define HAL_VM_H
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

#include <hal/kerntypes.h>
#include <target-hal/vm.h>

/** @file
 * @brief Functions to manipulate the master kernel map.
 */

/* *****************************************************************
 *
 * INTERFACES FOR KERNEL MAP MANAGEMENT
 *
 * *****************************************************************/

#define KMAP_R  0x1u
#define KMAP_W  0x2u
#define KMAP_X  0x4u
#define KMAP_NC 0x8u		/* non-cacheable */

/** @brief Ensure that a given kernel address is mappable.
 *
 * The architecture-specific subsystem should acquire any necessary
 * storage needed to ensure that the passed kernel address can later
 * be validly mapped without blocking. Once this has been called, the
 * specified kernel address must remain mappable indefinitely.
 *
 * THIS FUNCTION IS NOT SMP-SAFE. Only two types of code should call
 * this after multiprocessing is enabled:
 * - The grow_heap() function (in kern_malloc.c), and then only while
 *   holding the heap allocation lock.
 * - Code that is doing CPU-local mapping window management, and
 *   therefore knows that the updates are happening to CPU-local page
 *   directories and page tables.
 */
void kmap_EnsureCanMap(kva_t va, const char *descrip);

/** @brief Map a kernel virtual address to a kernel physical address.
 *
 * Panics if the appropriate PTE slot is not present. It is possible
 * that I will merge this with kmap_EnsureCanMap() in the future.
 *
 * THIS FUNCTION IS NOT SMP-SAFE. Only two types of code should call
 * this after multiprocessing is enabled:
 * - The grow_heap() function (in kern_malloc.c), and then only while
 *   holding the heap allocation lock.
 * - Code that is doing CPU-local mapping window management, and
 *   therefore knows that the updates are happening to CPU-local page
 *   directories and page tables.
 */
void kmap_map(kva_t va, kpa_t pa, uint32_t perms);

/** @brief Opaque PTE type definition. 
 *
 * Target-specific HAL must define TARGET_HAL_PTE_T. Machine
 * independent code may declare and use pointers to this type, but the
 * type itself (including its size) is fully opaque.
 */
typedef TARGET_HAL_PTE_T pte_t;

void pte_invalidate(pte_t *);

/** @brief Return the physical address of the CPU-private map for the
 * currently executing CPU. */
struct Mapping *vm_percpu_kernel_map(void);

/** @brief Switch address space register of the current CPU to the
 * provided PA. */
void vm_switch_curcpu_to_map(struct Mapping *map);

/** @brief Returns TRUE iff the specified VA is a valid user-mode VA
 * for the referencing process according to the address map of the current
 * architecture.
 */
struct Process;
static inline bool vm_valid_uva(struct Process *p, kva_t uva);

static inline void local_tlb_flush();

static inline void local_tlb_flushva(kva_t va);

#endif /* HAL_VM_KMAP_H */
