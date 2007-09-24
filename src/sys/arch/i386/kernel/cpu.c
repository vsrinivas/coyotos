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
 * @brief CPU initialization logic.
 */

#include <stddef.h>
#include <kerninc/printf.h>
#include <kerninc/malloc.h>
#include <kerninc/CPU.h>
#include <kerninc/PhysMem.h>
#include <kerninc/string.h>
#include <kerninc/pstring.h>
#include <kerninc/assert.h>
#include <hal/transmap.h>
#include "cpu.h"
#include "acpi.h"
#include "hwmap.h"

extern kva_t TransientMap[];

ArchCPU archcpu_vec[MAX_NCPU];


#if 0
static void
map_pa_in_table(kva_t tbl_va, kva_t page_va, kpa_t page_pa)
{
  if (UsingPAE) {
    uint32_t lndx = PAE_PGTBL_NDX((kva_t) page_va);
    IA32_PAE *pgtbl = (IA32_PAE *) tbl_va;
	
    pgtbl[lndx].value = 0;
    pgtbl[lndx].bits.frameno = PAE_KPA_TO_FRAME(page_pa);
    pgtbl[lndx].bits.V = 1;
    pgtbl[lndx].bits.W = 1;
    pgtbl[lndx].bits.PGSZ = 0;
    pgtbl[lndx].bits.ACC = 1;
    pgtbl[lndx].bits.DIRTY = 1;
    pgtbl[lndx].bits.PGSZ = 0;
  }
  else {
    uint32_t lndx = PTE_PGTBL_NDX((kva_t) page_va);
    IA32_PTE *pgtbl = (IA32_PTE *) tbl_va;
	
    pgtbl[lndx].value = 0;
    pgtbl[lndx].bits.frameno = PTE_KPA_TO_FRAME(page_pa);
    pgtbl[lndx].bits.V = 1;
    pgtbl[lndx].bits.W = 1;
    pgtbl[lndx].bits.PGSZ = 0;
    pgtbl[lndx].bits.ACC = 1;
    pgtbl[lndx].bits.DIRTY = 1;
    pgtbl[lndx].bits.PGSZ = 0;
  }
}

static void
make_cpu_local_page(kva_t tbl_va, void *src_va)
{
  kpa_t pg_pa = 
    pmem_AllocBytes(&pmem_need_pages, COYOTOS_PAGE_SIZE, 
		    pmu_KERNEL, "CPU Priv. Data");

  void * pg_va = TRANSMAP_MAP(pg_pa, void *);
  memcpy(pg_va, src_va, COYOTOS_PAGE_SIZE);

  map_pa_in_table(tbl_va, (kva_t)pg_va, pg_pa);

  TRANSMAP_UNMAP(pg_va);

}
#endif

size_t
cpu_probe_cpus(void)
{
  // FIX: On pre-ACPI, non-MP systems, we want to determine here
  // whether the primary CPU supports a local APIC, which means that
  // we want to call cpu_scan_features on CPU0 from here. We cannot
  // call cpu_scan_features on other CPUs until interrupts are
  // enabled.

  size_t ncpu = acpi_probe_cpus();
  if (ncpu)
    printf("ACPI reports %d CPUs\n", ncpu);
#if 0
  if (ncpu == 0)
    ncpu = mptable_probe_cpus();
#endif

  if (ncpu == 0)
    ncpu = 1;			/* we ARE running on SOMETHING */

  cpu_ncpu = ncpu;

  assert(cpu_ncpu <= MAX_NCPU);

  return cpu_ncpu;
}

void
cpu_vector_init(void)
{
  assert(cpu_ncpu <= MAX_NCPU);

#if 0
  /* For each configured CPU other than CPU0, we need to clone the
   * CPU-private region.
   *
   *   A CPU-local page table
   *   A CPU-local data area
   *   A CPU-local transmap
   */

  for (size_t i = 1; i < cpu_ncpu; i++) {
    printf("Setting up CPU %d\n", i);
    kpa_t tbl_pa = 
      pmem_AllocBytes(&pmem_need_pages, COYOTOS_PAGE_SIZE, 
		      pmu_KMAP, "CPU Priv. pgtbl");

    archcpu_vec[i].localDataPageTable = tbl_pa;

    void *tbl_va = TRANSMAP_MAP(tbl_pa, void *);

    /* For the most part, this page table looks like the main one: */
    memcpy(tbl_va, &KernPageTable, COYOTOS_PAGE_SIZE);

    char *localData = (char *) &_data_cpu;
    while (localData != (char *) &_edata_cpu) {
      make_cpu_local_page((kva_t)tbl_va, localData);
      localData += COYOTOS_PAGE_SIZE;
    }

#if 1
    kpa_t cpu_transmap =
      pmem_AllocBytes(&pmem_need_pages, COYOTOS_PAGE_SIZE, 
		      pmu_KMAP, "transmap");

    printf("Got transmap_pa 0x%llx\n", cpu_transmap);
    memset_p(cpu_transmap, 0, COYOTOS_PAGE_SIZE);

    archcpu_vec[i].transMapMappingPage = cpu_transmap;

    map_pa_in_table((kva_t)tbl_va, (kva_t)&TransientMap, cpu_transmap);
#endif

    TRANSMAP_UNMAP(tbl_va);
  }
#endif
}
