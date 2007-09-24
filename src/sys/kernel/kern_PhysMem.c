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

/** @file
 * @brief Physical memory management.
 *
 * Allocations of physical memory are guaranteed to be physically
 * contiguous. A physical region is not necessarily virtually mapped.
 *
 * In the EROS kernel, an early partition of memory was made to divide
 * the kernel into node space and page space. The heap was then
 * demand-extended by ``borrowing'' pages from page space. Borrowing
 * frames in this form is necessary, because we need to be able to
 * dynamically allocate object header vectors for device memory.
 * There is really no way to know in advance how big the device memory
 * will be, so the kernel needs a means to dynamically grow the
 * heap. The only way to do that is by borrowing frames from page
 * space.
 * 
 * In Coyotos, there is a larger set of object types (pages, PATTs,
 * processes, endpoints), I we don't yet know what the right
 * proportion of these is. I want to have the option to vary the
 * proportion of kernel data structures based on command line
 * directives, which means that it is helpful to allocate them from
 * the heap. This means that we want to be able to use the heap fairly
 * early in the bootstrap process.
 *
 * The way I have resolved this is to set up an initial arrangement in
 * which <em>every</em> non-kernel page frame is assigned to page
 * space, and then have the heap logic steal frames from that as
 * needed.
 *
 */

#include <stddef.h>
// #include <coyotos/machine/target.h>
#include <hal/config.h>
#include <kerninc/assert.h>
#include <kerninc/printf.h>
#include <kerninc/PhysMem.h>
#include <kerninc/shellsort.h>
#include <kerninc/string.h>
#include <kerninc/util.h>
     
#define DEBUG_PMEM if (0)

/** @brief Constraint to use when you require page aligned data. */
PmemConstraint pmem_need_pages = { 
  (kpa_t) 0u, ~((kpa_t)0u), COYOTOS_PAGE_SIZE, true };
/** @brief Constraint to use when you require word-aligned data. */
PmemConstraint pmem_need_words = { 
  (kpa_t) 0u, ~((kpa_t)0u), sizeof(uint32_t), true };
/** @brief Constraint to use when you require byte-aligned data. */
PmemConstraint pmem_need_bytes = {
  (kpa_t) 0u, ~((kpa_t)0u), sizeof(uint8_t), true };

static PmemInfo pmem_table[PHYSMEM_NREGION];
static unsigned nPmemInfo = 0;
// static unsigned pmem_table_sz = 0;


static void pmem_cleanup();
PmemInfo *pmem_FindRegion(kpa_t addr);
static PmemInfo *pmem_NewRegion(kpa_t base, kpa_t bound, PmemClass cls, 
				PmemUse use, const char *descrip);

/** @brief Initialize the physically address range. 
 *
 * On many architecture families, this cannot be determined at
 * compile time (e.g. Pentium family). On others, it needs to be
 * established by the particular board support package.
 *
 * Calling this routine establishes the initial "hole" that will
 * subsequently be split by all new region definitions.
 *
 * @invariant This function must be called before any other pmem_XXX()
 * routine is called.
 *
 * @bug The updates to the various constraints are probably not
 * needed. In theory, having failed to create any holes that are
 * outside these ranges, it should be impossible to allocate outside
 * them. I (shap) want to inspect the current code before assuming
 * this. In the interim, qualifying the base/bound here does no harm.
 */
void
pmem_init(kpa_t base, kpa_t bound)
{
  (void) pmem_NewRegion(base, bound, pmc_ADDRESSABLE, pmu_AVAIL, 0);

  pmem_need_pages.base = base;
  pmem_need_pages.bound = bound;
  pmem_need_words.base = base;
  pmem_need_words.bound = bound;
  pmem_need_bytes.base = base;
  pmem_need_bytes.bound = bound;
}


/** @brief Given a physical address, find the containing region.
 */
PmemInfo *
pmem_FindRegion(kpa_t addr)
{
  for (unsigned i = 0; i < PHYSMEM_NREGION; i++) {
    if (pmem_table[i].cls == pmc_UNUSED)
      continue;

    if (pmem_table[i].base <= addr && addr < pmem_table[i].bound)
      return &pmem_table[i];
  }

  return NULL;
}

/** @brief Add a new physical region to the kernel's physical memory
 * region list.
 *
 * Allocates an entry from the physical memory region table.
 */
PmemInfo *
pmem_NewRegion(kpa_t base, kpa_t bound, PmemClass cls, PmemUse use,
	       const char *descrip)
{
  if (nPmemInfo >= PHYSMEM_NREGION) {
    pmem_showall();
    fatal("Physical region list exhausted.\n");
  }

  assert(nPmemInfo < PHYSMEM_NREGION);
  assert(pmem_table[nPmemInfo].cls == pmc_UNUSED);

  PmemInfo *pmi = &pmem_table[nPmemInfo];
  nPmemInfo++;

  assert(pmi->cls == pmc_UNUSED);
  assert(pmi->use == pmu_AVAIL);
  assert(pmi->descrip == 0);
  assert(pmi->base == 0);
  assert(pmi->bound == 0);

  DEBUG_PMEM
    printf("Add physregion [%llx,%llx] %s %s\n", base, bound,
	   pmc_descrip(cls), pmu_descrip(use));

  {
    PmemInfo *alt = pmem_FindRegion(base);
    if (alt)
      fatal("  base overlaps with [0x%llx, 0x%llx] %s %u\n",
	    alt->base,
	    alt->bound,
	    pmc_descrip(cls), 
	    pmu_descrip(use));

    alt = pmem_FindRegion(bound-1);
    if (alt)
      fatal("  bound overlaps with [0x%llx, 0x%llx] %s %s\n",
	    alt->base,
	    alt->bound,
	    pmc_descrip(alt->cls), 
	    pmu_descrip(alt->use));
  }

  pmi->base = base;
  pmi->bound = bound;
  //  pmi->flags = flags;
  pmi->cls = cls;
  pmi->use = use;
  pmi->descrip = descrip;

  return pmem_FindRegion(base);
}

const char *
pmc_descrip(PmemClass cls)
{
  switch(cls) {
  case pmc_UNUSED:
    return "unused";
  case pmc_ADDRESSABLE:
    return "PADRES";
  case pmc_RAM:
    return "RAM";
  case pmc_NVRAM:
    return "NVRM";
  case pmc_ROM:
    return "ROM";

  default:
    assert(false);
    return 0;
  }
}

const char *
pmu_descrip(PmemUse use)
{
  switch(use) {
  case pmu_AVAIL:
    return "AVAIL";
  case pmu_BIOS:
    return "BIOS";
  case pmu_ACPI_NVS:
    return "ACPI_NVS";
  case pmu_ACPI_DATA:
    return "ACPI_DATA";
  case pmu_KERNEL:
    return "KERNEL";
  case pmu_ISLIMG:
    return "ISLIMG";
  case pmu_KHEAP:
    return "KHEAP";
  case pmu_KMAP:
    return "KMAP";
  case pmu_PAGES:
    return "PAGES";
  case pmu_DEV:
    return "DEV";
  case pmu_DEVPAGES:
    return "DEVPGS";
  case pmu_PGTBL:
    return "PGTBL";

  default:
    assert(false);
    return 0;
  }
}

/** @brief Allocate a portion of a physical region for some particular
 * purpose.
 */
kpa_t
pmem_AllocRegion(kpa_t base, kpa_t bound, PmemClass cls, PmemUse use,
		 const char *descrip)
{
  if (base == bound)
    return 0;

  assert(base < bound);
  PmemInfo *pmi = pmem_FindRegion(base);
  if (!pmi)
    return PMEM_ALLOC_FAIL;

  if (pmi->base > base ||
		 pmi->bound < bound ||
		 pmi->base > bound ||
		 pmi->bound < bound) {
    //    pmem_showall();
    fatal("?ALOC [%llx,%llx] %s %s %s\n"
	   "  PMI [%llx,%llx] %s %s\n", 
	  base, bound, pmc_descrip(cls), pmu_descrip(use),
	  descrip,
	  pmi->base, pmi->bound, pmc_descrip(pmi->cls),
	  pmu_descrip(pmi->use));
  }

  if (base != pmi->base) {
    assert (base > pmi->base);

    kpa_t obound = pmi->bound;
    pmi->bound = base;
    
    assert(base != obound);
    pmi = pmem_NewRegion(base, obound, pmi->cls, pmi->use, pmi->descrip);
  }

  assert(pmi->base == base);
  if (bound != pmi->bound) {
    assert(bound < pmi->bound);

    kpa_t obound = pmi->bound;
    pmi->bound = bound;

    assert(bound != obound);
    pmem_NewRegion(bound, obound, pmi->cls, pmi->use, pmi->descrip);
  }

  DEBUG_PMEM
    printf("ALC physregion [%llx,%llx] %s %s\n", base, bound,
	   pmc_descrip(cls), pmu_descrip(use));

  assert(pmi->base == base);
  assert(pmi->bound == bound);
  pmi->use = use;
  pmi->cls = cls;
  pmi->descrip = descrip;

  pmem_cleanup();

  return base;
}

/** @brief Dump the current region list to the console, if we have one.
 */
void
pmem_showall()
{
  for (unsigned i = 0; i < PHYSMEM_NREGION; i++) {
    if (pmem_table[i].cls == pmc_UNUSED)
      continue;

    printf("[0x%016llx, 0x%016llx] %-4s %-9s %s\n",
	   pmem_table[i].base,
	   pmem_table[i].bound,
	   pmc_descrip(pmem_table[i].cls),
	   pmu_descrip(pmem_table[i].use),
	   pmem_table[i].descrip ? pmem_table[i].descrip : "");
  }
}

/** @brief Given two regions, decide which one to allocate from
 * preferentially.
 *
 * On most machines it doesn't matter a bit, but on the PC it is
 * preferable to allocate out of higher memory, because legacy DMA
 * controllers tend to have restricted addressable bounds in physical
 * memory. */
static PmemInfo *
preferred_region(PmemInfo *rgn1, PmemInfo *rgn2)
{
  if (rgn1 == 0)
    return rgn2;

  if (rgn2 && rgn1->base < rgn2->base)
    return rgn2;

  return rgn1;
}

/** @brief Find a region from which to allocate nBytes of RAM that
 * satisfies the PmemConstraint. */
static PmemInfo *
pmem_ChooseRegion(size_t nBytes, const PmemConstraint *mc)
{
  PmemInfo *allocTarget = 0;

  for (unsigned i = 0; i < PHYSMEM_NREGION; i++) {
    PmemInfo *pmi = &pmem_table[i];

    if (pmi->cls != pmc_RAM)
      continue;
    if (pmi->use != pmu_AVAIL)
      continue;

    kpa_t base = max(pmi->base, mc->base);
    kpa_t bound = min(pmi->bound, mc->bound);

    base = align_up(base, mc->align);

    if (base >= bound)
      continue;

    /* The region (partially) overlaps the requested region. See if it
     * has enough suitably aligned space: */

    if (mc->fromTop) {
      kpa_t where = bound - nBytes;
      where = align_down(where, mc->align);

      if (where < base)
	continue;
    }
    else {
      kpa_t where = align_up(base, mc->align);

      if (where + nBytes > bound)
	continue;
    }

    allocTarget = preferred_region(allocTarget, pmi);
  }

  return allocTarget;
}

/** @brief Allocate some amount of contiguous physical memory, subject
 * to the constraint.
 * 
 * Allocates the requested amount of memory, subject to the
 * constraint. Returns the address if found, else -1u if no memory is
 * available. 
 */
kpa_t
pmem_AllocBytes(const PmemConstraint *mc, size_t nBytes,
		PmemUse use, const char *descrip)
{
  PmemInfo *pmi = pmem_ChooseRegion(nBytes, mc);
  if (!pmi)
    return PMEM_ALLOC_FAIL;

  /* Apply the constraint. */
  kpa_t bound = min(pmi->bound, mc->bound);
  kpa_t base =  max(pmi->base, mc->base);

  kpa_t where;

  if (mc->fromTop) {
    /* The highest possible address is the bound, less nBytes, aligned
       downwards to satisfy the alignment constraint. */
    where = align_down(bound - nBytes, mc->align);
    assert(where >= pmi->base);
  }
  else {
    /* The lowest possible address is the base, aligned upwards to
       satisfy the alignment constraint. */
    where = align_up(base, mc->align);
    assert(where + nBytes <= pmi->bound);
  }

  return pmem_AllocRegion(where, where + nBytes, pmc_RAM, use, descrip);
}

/** @brief Return the number of available units, each of size
 * unitSize.
 *
 * If contiguous is true, this returns the maximum number of available
 * contiguous units of the specified size.
 *
 * There is a problem with the return type: on a 32 bit machine with
 * an extended physical memory size (e.g. IA-32), this interface
 * cannot be used directly to inquire about the number of available
 * pages. If you have 64G of physical memory, and you ask about a data
 * structure whose size is < 32 bytes, you are going to get a result
 * that cannot be expressed within the representable range of a size_t.
 */
kpsize_t
pmem_Available(const PmemConstraint *mc, kpsize_t unitSize, bool contiguous)
{
  kpsize_t nUnits = 0;
  kpsize_t nContigUnits = 0;

  for (unsigned i = 0; i < PHYSMEM_NREGION; i++) {
    PmemInfo *pmi = &pmem_table[i];

    if (pmi->cls != pmc_RAM)
      continue;
    if (pmi->use != pmu_AVAIL)
      continue;

    kpa_t base = max(pmi->base, mc->base);
    kpa_t bound = min(pmi->bound, mc->bound);

    base = align_up(base, mc->align);

    if (base >= bound)
      continue;

    kpsize_t unitsHere = (bound -= base) / unitSize;

    if (nContigUnits < unitsHere)
      nContigUnits = unitsHere;

    nUnits += unitsHere;
  }

  return contiguous ? nContigUnits : nUnits;
}


/** @brief Comparison function for re-sorting the physmem structures.
 *
 * There is one sleazy trick here, which is that entries with type
 * pmat_FREE sort <em>higher</em> than anything else. This ensures
 * that they end up at the top of the vector.
 */
static int 
cmp_base(const void *vp1, const void *vp2)
{
  const PmemInfo *pmi1 = vp1;
  const PmemInfo *pmi2 = vp2;

  assert((pmi1->cls == pmc_UNUSED) || (pmi1->bound > pmi1->base));
  assert((pmi2->cls == pmc_UNUSED) || (pmi2->bound > pmi2->base));

  if (pmi1->cls == pmc_UNUSED && pmi2->cls != pmc_UNUSED)
    return 1;
  if (pmi1->cls != pmc_UNUSED && pmi2->cls == pmc_UNUSED)
    return -1;

  // Either both are pmc_UNUSED or neither is. In those cases sort by
  // the base addresses:

  if (pmi1->base < pmi2->base)
    return -1;
  else if (pmi1->base > pmi2->base)
    return 1;
  return 0;
}

static inline bool
mergeable(PmemUse u)
{
  if (u == pmu_DEV) return false;
  if (u == pmu_DEVPAGES) return false;

  return true;
}

static bool
same_descrip(const char *s1, const char *s2)
{
  if (s1 == s2)
    return true;
  if (s1 && s2)
    return (strcmp(s1, s2) == 0);
  return false;
}


static void
pmem_cleanup()
{
  shellsort(pmem_table, PHYSMEM_NREGION, sizeof(*pmem_table), cmp_base);

  /* Check for mergeable regions: */
  size_t ndx = 0;
  while (ndx < nPmemInfo - 1) {
    PmemInfo *pmi1 = &pmem_table[ndx];
    PmemInfo *pmi2 = &pmem_table[ndx+1];

    assert (pmi1->cls != pmc_UNUSED);
    assert (pmi2->cls != pmc_UNUSED);

    // By construction, bases and bounds should always end up adjacent
    // after sorting.
    assert (pmi1->bound == pmi2->base);

    if (pmi1->cls == pmi2->cls && /* classes must match */
	pmi1->use == pmi2->use && /* uses must match */
	mergeable(pmi1->use) &&	/* uses must be mergeable */
	mergeable(pmi2->use) &&
	same_descrip(pmi1->descrip, pmi2->descrip)
	) {

      DEBUG_PMEM {
	printf("Merging:\n");
	printf("  [0x%016llx, 0x%016llx] %s %s %s\n",
	       pmi1->base,
	       pmi1->bound,
	       pmc_descrip(pmi1->cls),
	       pmu_descrip(pmi1->use),
	       pmi1->descrip ? pmi1->descrip : "");
	printf("  [0x%016llx, 0x%016llx] %s %s %s\n",
	       pmi2->base,
	       pmi2->bound,
	       pmc_descrip(pmi2->cls),
	       pmu_descrip(pmi2->use),
	       pmi2->descrip ? pmi2->descrip : "");
      }

      // These two regions should be merged.
      pmi1->bound = pmi2->bound;
      INIT_TO_ZERO(pmi2);

      shellsort(pmem_table, PHYSMEM_NREGION, sizeof(*pmem_table), cmp_base);

      nPmemInfo --;
      DEBUG_PMEM {
	printf("Result:\n");
	printf("  [0x%016llx, 0x%016llx] %s %s %s (%d)\n",
	       pmi1->base,
	       pmi1->bound,
	       pmc_descrip(pmi1->cls),
	       pmu_descrip(pmi1->use),
	       pmi1->descrip ? pmi1->descrip : "",
	       nPmemInfo);
      }

      continue;
    }

    ndx++;
  }
}



#if 0
/** @brief Remove all knowledge of a physical memory region from the
 * kernel.
 */
void
pmem_DropRegion(PmemInfo *pmi)
{
  pmi->type = pmat_FREE;

  // We have changed the region table to have some new
  // entries. Re-sort it.
  pmem_cleanup();
}

/** @brief Free an allocated region.
 *
 * @todo This is WRONG!
 */
void
pmem_FreeRegion(PmemInfo *pmi)
{
#if 0
  // We are freeing a region, which causes it to revert back to its
  // state before allocation.
  if (pmi->flags & pmfl_ROM) {
    pmi->type = pmat_ROM;
    pmi->descrip = "ROM";
  }
  else {
    pmi->type = pmat_RAM;
    pmi->descrip = "RAM";
  }
#endif

  // See if we can coalesce this region into its successor:
  size_t ndx = pmi - pmem_table;
  if ((ndx < (PHYSMEM_NREGION - 1))
      && pmi[0].type == pmi[1].type
      && pmi[0].bound == pmi[1].base) {
    pmi[0].bound = pmi[1].bound;
    pmi[1].type = pmat_FREE;
  }

  // See if we can coalesce this region with its predecessor:
  if (ndx > 0) {
    pmi--;

    if (pmi[0].type == pmi[1].type
	&& pmi[0].bound == pmi[1].base) {
      pmi[0].bound = pmi[1].bound;
      pmi[1].type = pmat_FREE;
    }
  }

  // We have changed the region table to have some new
  // entries. Re-sort it.
  pmem_cleanup();
}

#endif
