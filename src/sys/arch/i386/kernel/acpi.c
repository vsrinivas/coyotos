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
 *
 * @brief Minimal ACPI bootstrap support.
 *
 * The Coyotos kernel does not directly support ACPI; that is an
 * application matter. We do need to do a small bit of ACPI scanning
 * in order to configure the CPUs in SMP configurations.
 *
 * The implementation here is based on ACPI Specification 3.0a, dated
 * December 30, 2005. I (shap) have not checked the ACPI 1.0 spec for
 * relevant differences.
 */

#include <stdint.h>
#include <stddef.h>
#include <hal/config.h>
#include <hal/kerntypes.h>
#include <hal/vm.h>
#include <kerninc/string.h>
#include <kerninc/pstring.h>
#include <kerninc/util.h>
#include <kerninc/printf.h>
#include <kerninc/assert.h>
#include <kerninc/PhysMem.h>

#include "kva.h"
#include "acpi.h"
#include "PIC.h"
#include "cpu.h"
#include "ioapic.h"
#include "lapic.h"

static uint8_t
acpi_cksum(kpa_t pa, size_t len)
{
  uint8_t sum = 0;

  while (len) {
    uintptr_t offset = pa & COYOTOS_PAGE_ADDR_MASK;
    pa -= offset;

    char *va = TRANSMAP_MAP(pa, char *);
    char *ptr = va + offset;
    kpa_t nBytes = min(len, COYOTOS_PAGE_SIZE - offset);

    for (size_t i = 0; i < nBytes; i++)
      sum += ptr[i];

    TRANSMAP_UNMAP(va);

    len -= nBytes;
  }

  return sum;
}

static kpa_t
acpi_scan_region(kpa_t ptr, kpa_t bound)
{
  while (ptr < bound) {
    RSDP_v3 *rsdp_va = PTOKV(ptr, RSDP_v3 *);
    kpa_t rsdp_pa = ptr;
    ptr += 16;

    if (memcmp((char *) rsdp_va->signature, "RSD PTR ", 8) != 0)
      continue;

    /* Test the v1.0 checksum */
    if (acpi_cksum(rsdp_pa, 20))
      continue;

    /* Test the v3.0 checksum */
    if (rsdp_va->revision >= 2 && acpi_cksum(rsdp_pa, rsdp_va->length))
      continue;

    /* Checksum checks out. Test if the rsdt pointer actually points
     * as an RSDT structure by testing that signature. */
    char rsdt_sig[4];
    memcpy_ptov(&rsdt_sig, rsdp_va->rsdt, 4);
    if (memcmp(rsdt_sig, "RSDT", 4) != 0)
      continue;

    /* Found it (we think): */
    return rsdp_pa;
  }

  return 0;			/* not found */
}

/** @brief Locate the Multiple APIC Description Table */
static kpa_t 
acpi_find_MADT()
{
  static kpa_t madt_pa = 0;

  if (madt_pa)
    return madt_pa;

  /* First need to find the RSDP. ACPI 3.0a/5.2.5.1 says that it must
   * either be:
   *
   *   Within the first 1024 bytes of the EBDA, or
   *   Somewhere in the BIOS ROM betweeon 0xE0000 and 0xFFFFF.
   *
   * In the latter case, we must presumably be talking about one of
   * the simple, fixed machine configurations.
   *
   * Note there is no need to kmap either of the RSDP search regions,
   * because both fall within the low 2M, and that is part of the
   * primordial map. At the point where this routine is called, we
   * have not yet switched to an application page table, so it is
   * still okay to rely on that map.
   */

  kpa_t ebda = ( (* PTOKV(0x40e, uint16_t *)) << 4 );

  kpa_t rsdp = acpi_scan_region(ebda, ebda + 1024);
  if (!rsdp) rsdp = acpi_scan_region(0xe0000, 0xfffff);
  if (!rsdp) return 0;
    
  printf("ACPI RSDP 0x%08x\n", (uint32_t) rsdp);

  /* ACPI RSDP has been found. This gives us location of RSDT. 
   *
   * We now need to search RSDT to find Multiple APIC Descriptor Table
   * (MADT).
   *
   * Searching RSDT is pain, because we cannot assume that it falls
   * within the low 2M. In particular, it may (in some cases) fall
   * within the high-memory ROM map.
   */
  kpa_t rsdt_pa = PTOKV(rsdp, RSDP_v3 *)->rsdt;
  SDT_hdr madt_hdr;
  SDT_hdr rsdt_hdr;

  memcpy_ptov(&rsdt_hdr, rsdt_pa, sizeof(rsdt_hdr));

  if (acpi_cksum(rsdt_pa, rsdt_hdr.length))
    fatal("ACPI checksum on RSDT fail\n");

  size_t nEntries = (rsdt_hdr.length - sizeof(rsdt_hdr)) / 4;

  printf("ACPI RSDT 0x%08x %d nEntries=%d\n", (uint32_t) rsdt_pa, 
	rsdt_hdr.length, nEntries);

  for (size_t i = 0; i < nEntries; i++) {
    kpa_t pa;
    uint32_t pa32;

    memcpy_ptov(&pa32, rsdt_pa + sizeof(rsdt_hdr) + (4 * i), sizeof(uint32_t));
    pa = pa32;

    memcpy_ptov(&madt_hdr, pa, sizeof(madt_hdr));

    /* What the hell: */
    printf("ACPI %c%c%c%c 0x%08x %d\n",
	   madt_hdr.signature[0],
	   madt_hdr.signature[1],
	   madt_hdr.signature[2],
	   madt_hdr.signature[3],
	   (uint32_t) pa, madt_hdr.length);

    if (acpi_cksum(pa, madt_hdr.length)) {
      printf("  [ checksum fail ]\n");
    }
    else if (memcmp(madt_hdr.signature, "APIC", 4) == 0) {
      /* Some BIOS's have been observed in the wild with multiple APIC
	 tables. Believe only the first one: */
      if (!madt_pa) madt_pa = pa;
    }
  }
    
  return madt_pa;
}

bool
acpi_probe_apics()
{
  uint32_t madt_pa = acpi_find_MADT();

  if (madt_pa == 0)
    return false;

  memcpy_ptov(&lapic_pa, madt_pa + sizeof(SDT_hdr), sizeof(uint32_t));

  assert(lapic_pa != 0);
  pmem_AllocRegion(lapic_pa, lapic_pa + 4096,
		   pmc_RAM, pmu_KERNEL, "LAPIC");

  kmap_EnsureCanMap(I386_LOCAL_APIC_VA, "lapic");
  kmap_map(I386_LOCAL_APIC_VA, lapic_pa, KMAP_R|KMAP_W|KMAP_NC);
  lapic_va = I386_LOCAL_APIC_VA;

  {
    uint32_t multiple_apic_flags;
    memcpy_ptov(&multiple_apic_flags, madt_pa + sizeof(SDT_hdr) + 4, 
		sizeof(uint32_t));

    if (multiple_apic_flags & 0x1u) {
      printf("8259 must be disabled on lapic start.\n");
      lapic_requires_8259_disable = true;
    }
  }

  SDT_hdr madt_hdr;

  memcpy_ptov(&madt_hdr, madt_pa, sizeof(madt_hdr));

  kpa_t madt_bound = madt_pa + madt_hdr.length;
  kpa_t apicstruct_pa = madt_pa + sizeof(madt_hdr) + 8;

  CpuIOAPIC ioapic;

  bool found = false;

  size_t nIoAPIC = 0;

  /* Issue: ACPI does not specify any ordering for entrys in the APIC
   * table. This creates a problem, because we do not know if the
   * first entry in the table corresponds to the IPL CPU.
   *
   * To ensure that logical CPU ID 0 is assigned to the boot
   * processor, handle slot 0 as a special case.
   */
  for(kpa_t pa = apicstruct_pa; pa < madt_bound; pa += ioapic.length) {
    memcpy_ptov(&ioapic, pa, 2);

    assert(ioapic.length);

    if (ioapic.type == LAPIC_type_IO_APIC) {
      memcpy_ptov(&ioapic, pa, sizeof(ioapic));

      kpa_t ioapic_pa = ioapic.ioapicPA;
      assert(ioapic_pa != 0);
      pmem_AllocRegion(ioapic_pa, ioapic_pa + 4096,
		       pmc_RAM, pmu_KERNEL, "IOAPIC");

      kva_t va = I386_IO_APIC_VA + (nIoAPIC * COYOTOS_PAGE_SIZE);
      kmap_EnsureCanMap(va, "ioapic");
      kmap_map(va, ioapic_pa, KMAP_R|KMAP_W|KMAP_NC);

      ioapic_register(ioapic.globalSystemInterruptBase, va);

      printf("IOAPIC id=0x%02x at 0x%0p intBase %d\n",
	     ioapic.ioapicID, ioapic.ioapicPA, 
	     ioapic.globalSystemInterruptBase);

      found = true;
    }
  }

  return found;
}

bool
acpi_map_interrupt(irq_t irq, IntSrcOverride *ret_isovr)
{
  IntSrcOverride isovr;

  /* Set up the default response that is appropriate to the bus type
   * for this IRQ. */
  ret_isovr->type = LAPIC_type_Intr_Source_Override;
  ret_isovr->length = sizeof(isovr);
  ret_isovr->source = IRQ_PIN(irq);
  ret_isovr->flags = MPS_INTI_POLARITY_FROMBUS|MPS_INTI_TRIGGER_FROMBUS;
  ret_isovr->globalSystemInterrupt = IRQ_PIN(irq); /* until remapped  */

  switch (IRQ_BUS(irq)) {
  case IBUS_ISA:
    {
      /* ISA bus was edge triggered, active high */
      ret_isovr->bus = ACPI_BUS_ISA;
      ret_isovr->flags = MPS_INTI_TRIGGER_EDGE | MPS_INTI_POLARITY_ACTIVEHI;
      break;
    }
  default:
    {
      fatal("Unknown input bus type to acpi_map_interrupt.\n");
    }
  }

  uint32_t madt_pa = acpi_find_MADT();

  if (madt_pa == 0)
    return false;

  SDT_hdr madt_hdr;

  memcpy_ptov(&madt_hdr, madt_pa, sizeof(madt_hdr));

  kpa_t madt_bound = madt_pa + madt_hdr.length;
  kpa_t apicstruct_pa = madt_pa + sizeof(madt_hdr) + 8;

  /* Issue: ACPI does not specify any ordering for entrys in the APIC
   * table. This creates a problem, because we do not know if the
   * first entry in the table corresponds to the IPL CPU.
   *
   * To ensure that logical CPU ID 0 is assigned to the boot
   * processor, handle slot 0 as a special case.
   */
  for(kpa_t pa = apicstruct_pa; pa < madt_bound; pa += isovr.length) {
    memcpy_ptov(&isovr, pa, 2);

    assert(isovr.length);

    if (isovr.type == LAPIC_type_Intr_Source_Override) {
      memcpy_ptov(&isovr, pa, sizeof(isovr));

      if ((IRQ_BUS(irq) == IBUS_ISA) && (isovr.bus != ACPI_BUS_ISA))
	continue;

      if (isovr.source != IRQ_PIN(irq))
	continue;

      ret_isovr->globalSystemInterrupt = isovr.globalSystemInterrupt;

      if ((isovr.flags & MPS_INTI_POLARITY) != MPS_INTI_POLARITY_FROMBUS) {
	ret_isovr->flags &= ~MPS_INTI_POLARITY;
	ret_isovr->flags |= (isovr.flags & MPS_INTI_POLARITY);
      }

      if ((isovr.flags & MPS_INTI_TRIGGER) != MPS_INTI_TRIGGER_FROMBUS) {
	ret_isovr->flags &= ~MPS_INTI_TRIGGER;
	ret_isovr->flags |= (isovr.flags & MPS_INTI_TRIGGER);
      }

      return true;
    }
  }

  return false;
}

size_t 
acpi_probe_cpus()
{
  size_t ncpu = 0;
  uint32_t madt_pa = acpi_find_MADT();

  if (madt_pa == 0)
    return ncpu;

  cpuid_regs_t regs;
  (void) cpuid(1u, &regs);
  uint8_t bootApicID = FIELD(regs.ebx, 31, 24);

  SDT_hdr madt_hdr;

  memcpy_ptov(&madt_hdr, madt_pa, sizeof(madt_hdr));

  kpa_t madt_bound = madt_pa + madt_hdr.length;
  kpa_t apicstruct_pa = madt_pa + sizeof(madt_hdr) + 8;

  CpuLocalAPIC lapic;

  /* Issue: ACPI does not specify any ordering for entrys in the APIC
   * table. This creates a problem, because we do not know if the
   * first entry in the table corresponds to the IPL CPU.
   *
   * To ensure that logical CPU ID 0 is assigned to the boot
   * processor, handle slot 0 as a special case.
   */
  ncpu = 1;
  for(kpa_t pa = apicstruct_pa; pa < madt_bound; pa += lapic.length) {
    memcpy_ptov(&lapic, pa, 2);

    assert(lapic.length);

    if (lapic.type == LAPIC_type_Processor_Local_APIC) {
      memcpy_ptov(&lapic, pa, sizeof(lapic));

      if ((lapic.flags & LAPIC_flag_Enabled) == 0)
	continue;

      size_t slot = ncpu;
      if (lapic.lapicID == bootApicID)
	slot = 0;

      archcpu_vec[slot].lapic_id = lapic.lapicID;
      cpu_vec[slot].present = true;

      // printf("Entry %d has lapic ID 0x%x\n", slot, lapic.lapicID);

      if (slot != 0) ncpu++;
    }
  }

  return ncpu;
}
