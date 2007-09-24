#ifndef __KERNINC_PHYSMEM_H__
#define __KERNINC_PHYSMEM_H__
/*
 * Copyright (C) 2007, The EROS Group, LLC.
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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <hal/kerntypes.h>
#include <hal/config.h>

/**
 * @brief Description of physical memory allocation constraint.
 *
 * Descriptor for a physical memory allocation constraint. Eventually,
 * this needs to become more detailed, because it needs to describe
 * boundary crossing issues that relate to DMA. At the moment I'm not
 * trying to deal with that.
 * 
 * The current kernel implements only three general memory
 * constraints: byte aligned at any address (high), word aligned at
 * any address (high) and page aligned at any address (high). The
 * general alignment and crossing constraint mechanism is currently
 * unimplemented, but the notion of a constraint structure is included
 * in anticipation of future ports that may require it.
 *
 */
struct PmemConstraint {
  kpa_t base;
  kpa_t bound;
  unsigned align;
  bool  fromTop;	    /* Allocate from top (true) or bottom
			       (false) of region. */
} ;
typedef struct PmemConstraint PmemConstraint;

/** @brief Classes of memory region.
 *
 * @note We may some day want to introduce pmc_DEVIO to describe a
 * memory-mapped I/O address range.
 *
 * @bug This enumeration is a temporary expedient. It eventually needs
 * to be defined by IDL, because physical memory region creation is an
 * interface that is externalized to drivers.
 *
 * @invariant pmc_UNUSED must be zero because the initial vector of
 * PmemInfo structures lives in BSS.
 *
 * @note If you update this, you need to update pmc_descrip() in
 * kern_PhysMem.c. 
 */
enum PmemClass {
  pmc_UNUSED = 0,   /**<! @brief Unused (free) physical memory region 
		     * record. */

  pmc_ADDRESSABLE,  /**<! @brief Undefined region of the addressable
		     * physical memory range.
		     *
		     * The initial memory region is pmc_ADDRESSABLE
		     * range consisting of the entire addressable
		     * physical space. Subsequent regions are
		     * allocated by splitting and defining this
		     * initial region.
		     */

  pmc_RAM,	    /**<! @brief Allocatable RAM region. */
  pmc_NVRAM,	    /**<! @brief Non-volatile RAM region. */
  pmc_ROM,	    /**<! @brief ROM region. */
  pmc_DEV,	    /**<! @brief Memory-mapped hardware device. */
};
typedef enum PmemClass PmemClass;

/** @brief Physical memory usage types.
 *
 * These describe the different types of uses to which a defined
 * physical memory region can be put.
 *
 * Regions whose use-type is pmu_DEV or pmu_DEVPAGES are not
 * mergeable, because they can be deleted as a consequence of device
 * removal. Other regions are not deallocatable, and can therefore be
 * merged provided provided the underlying memory device class and the
 * supplied description is the same. Note this implies that pmc_HOLE
 * regions are mergeable, and that (pmc_RAM, pmu_AVAIL) regions (that
 * is: free regions) are mergeable. This reduces pressure on the
 * global PmemInfo table.
 *
 * @bug This enumeration is a temporary expedient. Parts of it either
 * need to be moved or reflected (probably reflected) in IDL, because
 * physical memory region creation is an interface that is
 * externalized to drivers.
 *
 * @todo The presence of architecture-specific entries suggests that
 * this enum may need to be re-framed in a more flexible way.
 *
 * @invariant pmu_AVAIL must be zero because the initial vector of
 * PmemInfo structures lives in BSS.
 *
 * @note If you update this, you need to update pmu_descrip() in
 * kern_PhysMem.c. 
 */
enum PmemUse {
  // Following uses are mergeable provided their description fields
  // compare equal and their underlying memory device class is the
  // same.
  pmu_AVAIL = 0,    /**<! @brief Allocatable RAM region. */

  // Following uses are not deallocatable.
  pmu_BIOS,	    /**<! @brief BIOS-owned region. */
  pmu_ACPI_NVS,	    /**<! @brief ACPI non-volatile store (x86) */
  pmu_ACPI_DATA,    /**<! @brief ACPI data region (x86) */
  pmu_KERNEL,	    /**<! @brief Allocated to kernel */
  pmu_ISLIMG,	    /**<! @brief Initial system load image */
  pmu_KHEAP,	    /**<! @brief Kernel heap backing frame. */
  pmu_KMAP,	    /**<! @brief Kernel mapping frame. */
  pmu_PAGES,	    /**<! @brief Used by page space. */
  pmu_PGTBL,	    /**<! @brief User page tables or directories. */

  // Following uses are not mergeable because they can be deleted:
  pmu_DEV,	    /**<! @brief Region owned by a device. */
  pmu_DEVPAGES,	    /**<! @brief Device page space. */
};
typedef enum PmemUse PmemUse;

/** @brief Descriptor for a physically contiguous region of physical
 *  memory.
 *
 * In contrast to the older, EROS implementation, the Coyotos physical
 * region vector is byte based and sorted, and we do not suballocate
 * regions.
 */
struct PmemInfo {
  kpa_t     base;   /**<! @brief inclusive start of this range */
  kpa_t     bound;  /**<! @brief exclusive bound of this range */
  PmemClass cls;    /**<! @brief Memory device type (ram, rom, nvram...)  */
  PmemUse   use;    /**<! @brief How this range is being used. */

  const char *descrip; /**<! Debugging description.  */
} ;

typedef struct PmemInfo PmemInfo;

extern PmemConstraint pmem_need_pages;
extern PmemConstraint pmem_need_words;
extern PmemConstraint pmem_need_bytes;

void pmem_init(kpa_t base, kpa_t bound);

const char *pmc_descrip(PmemClass cls);
const char *pmu_descrip(PmemUse use);

#define PMEM_ALLOC_FAIL (~((kpa_t) 0))

kpa_t      pmem_AllocRegion(kpa_t base, kpa_t bound, PmemClass cls, 
			    PmemUse use, const char *descrip);

kpa_t      pmem_AllocBytes(const PmemConstraint *, size_t sz, 
			   PmemUse pmu, const char *descrip);

kpsize_t   pmem_Available(const PmemConstraint *, kpsize_t unitSize, 
			  bool contiguous);

PmemInfo * pmem_FindRegion(kpa_t addr);

void       pmem_showall();

/* Local Variables: */
/* comment-column:20 */
/* End: */

#endif /* __KERNINC_PHYSMEM_H__ */
