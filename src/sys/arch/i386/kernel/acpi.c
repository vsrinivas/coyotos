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

typedef struct RSDP_v3 {
  char     signature[8];
  uint8_t  cksum;
  char     oemid[6];
  uint8_t  revision;
  uint32_t rsdt;		/* physical address of RSDT */
  uint32_t length;		/* of entire RSDT */

  /* Fields below appeared starting in v3.0 */
  uint64_t xsdt;		/* Physical address of XSDT */
  uint8_t  ex_cksum;		/* Extended checksum */
  char     reserved[3];
} RSDP_v3;

typedef struct SDT_hdr {
  char     signature[4];
  uint32_t length;
  uint8_t  revision;
  uint8_t  cksum;
  char     oemid[6];
  char     oemTableID[8];
  uint32_t oemRevision;
  uint32_t creatorID;
  uint32_t creatorRevision;
} SDT_hdr;

#define LAPIC_type_Processor_Local_APIC 0x0
#define LAPIC_type_IO_APIC              0x1u
#define LAPIC_type_Intr_Source_Override 0x2u
#define LAPIC_type_NMI_Source           0x3u
#define LAPIC_type_LAPIC_NMI_Struct     0x4u
#define LAPIC_type_LAPIC_addr_override  0x5u
#define LAPIC_type_IO_SAPIC             0x6u
#define LAPIC_type_LOCAL_SAPIC          0x7u
#define LAPIC_type_Platform_Intr_Source 0x8u
/* 9   - 127 Reserved */
/* 128 - 255 Reserved for OEM use */

#define LAPIC_flag_Enabled 0x1u	/* CPU enabled/disabled */

typedef struct CpuLocalAPIC {
  uint8_t   type;
  uint8_t   length;
  uint8_t   acpiProcessorID;
  uint8_t   lapicID;
  uint32_t  flags;
} CpuLocalAPIC;

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

  for (size_t i = 0; i < nEntries; i++) {
    kpa_t pa;
    uint32_t pa32;

    memcpy_ptov(&pa32, rsdt_pa + sizeof(rsdt_hdr) + (4 * i), sizeof(uint32_t));
    pa = pa32;

    memcpy_ptov(&madt_hdr, pa, sizeof(madt_hdr));

    /* What the hell: */
    if (acpi_cksum(pa, madt_hdr.length))
      fatal("ACPI checksum on some table fail\n");

    if (memcmp(madt_hdr.signature, "APIC", 4) != 0)
      continue;

    madt_pa = pa;
    break;
  }
    
  if (madt_pa == 0) return 0;

  memcpy_ptov(&lapic_pa, madt_pa + sizeof(madt_hdr), sizeof(uint32_t));

  assert(lapic_pa != 0);
  pmem_AllocRegion(lapic_pa, lapic_pa + 4096,
		   pmc_RAM, pmu_KERNEL, "LAPIC");
  {
    uint32_t multiple_apic_flags;
    memcpy_ptov(&multiple_apic_flags, madt_pa + sizeof(madt_hdr) + 4, 
		sizeof(uint32_t));

    if (multiple_apic_flags & 0x1u) {
      printf("8259 must be disabled on lapic start.\n");
      lapic_requires_8259_disable = true;
    }
  }

  return madt_pa;
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
      // printf("Entry %d has lapic ID 0x%x\n", slot, lapic.lapicID);

      if (slot != 0) ncpu++;
    }
  }

  return ncpu;
}
