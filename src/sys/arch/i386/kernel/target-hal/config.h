#ifndef I386_HAL_CONFIG_H
#define I386_HAL_CONFIG_H
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
 * @brief Definitions of various kernel constants.
 */

#ifndef MAX_NCPU
/** @brief Maximum (static) number of CPUs we will support */
#define MAX_NCPU 4
#endif

#if MAX_NCPU > 32
#error "This kernel implementation does not support more than 32 CPUs"
#endif

/* Note that spacing is significant in the TRANSMAP_xxx macros. These
 * macros are referenced by the linker script ldscript.S, and the
 * linker script parser is very picky about tokenization. */

/** @brief Number of transmap entries reserved for each CPU. */
#define TRANSMAP_ENTRIES_PER_CPU 64
/** @brief Number of CPUS supported by a single transmap page.
 *
 * Note this assumes PAE mappings. PTE mappings can handle 1024 per
 * page, but we need to decide this statically here, so choose the
 * conservative outcome. */
#define TRANSMAP_CPUS_PER_PAGE (512 / TRANSMAP_ENTRIES_PER_CPU)
/** @brief Number of transmap pages we require, given the selected
 * value of MAX_NCPU */
#define TRANSMAP_PAGES  ((MAX_NCPU + TRANSMAP_CPUS_PER_PAGE - 1) / TRANSMAP_CPUS_PER_PAGE)

/** @brief Whether we have a console.
 */
#define HAVE_CONSOLE 1

/** @brief Number of entries in the physical region list.
 */
#define PHYSMEM_NREGION 512

/** @brief Set to the number of bits needed to index a particular PTE
 * in an arbitrary mapping table.
 *
 * This should be the width, in bits, of the index into the @em widest
 * page table on the current architecture.
 */
#define MAPPING_INDEX_BITS 10

/** @brief Size of a field that can hold a value @lt;=
 * MAPPING_INDEX_BITS 
 *
 * Not referenced if MAPPING_INDEX_BITS is 0
 */
#define L2_MAPPING_INDEX_BITS 4

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
 *
 * For IA-32, we re-map the local APIC at 0xFF000000, so don't go
 * above that.
 */

#define HEAP_LIMIT_VA            0xFF000000

/* These are not part of the HAL, and probably should not be allowed
 * to pollute the machine-independent namespace, but it seems silly to
 * create a whole separate header file for them. Further, I have
 * already mis-adjusted HEAP_LIMIT_VA once because these were not
 * documented here, and I would like not to do that again.
 *
 * Note the use of MAX_NCPU here to establish a gap between top of
 * heap and the CPU stacks. Since the stack for CPU0 is preallocated,
 * the region reserved here for CPU0 will never be mapped.
 */
#define SMP_STACK_VA             (HEAP_LIMIT_VA)

#define I386_LOCAL_APIC_VA       (SMP_STACK_VA + (MAX_NCPU*KSTACK_NPAGES*COYOTOS_PAGE_SIZE))
#define I386_IO_APIC_VA          (I386_LOCAL_APIC_VA+4096)

/** @brief Virtual base address of transient map. MUST be a multiple of 4M */
#define TRANSMAP_WINDOW_KVA      0xFF800000

/* Following check assumes PAE mode. If we are actually running in PTE
 * mode, the transmap_init() logic will map half as many page tables,
 * so the computation here remains correct in that case.
 */
#if ((0xffffffff - (TRANSMAP_NPAGES * COYOTOS_PAGE_SIZE)) + 1) < TRANSMAP_WINDOW_KVA
#error "TRANSMAP_WINDOW_KVA and HEAP_LIMIT_VA require adjustment for this many CPUs"
#endif

#endif /* I386_HAL_CONFIG_H */
