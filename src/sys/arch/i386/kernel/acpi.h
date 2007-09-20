#ifndef __I386_ACPI_H__
#define __I386_ACPI_H__
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
 */
#include <stddef.h>
#include <hal/irq.h>

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

typedef struct CpuIOAPIC {
  uint8_t   type;
  uint8_t   length;
  uint8_t   ioapicID;
  uint8_t   reserved;
  uint32_t  ioapicPA;
  uint32_t  globalSystemInterruptBase;
} CpuIOAPIC;

#define ACPI_BUS_ISA 0

#define MPS_INTI_POLARITY       0x0003u
#define   MPS_INTI_POLARITY_FROMBUS   0x00
#define   MPS_INTI_POLARITY_ACTIVEHI  0x01
#define   MPS_INTI_POLARITY_ACTIVELOW 0x03
#define MPS_INTI_TRIGGER        0x000Cu
#define   MPS_INTI_TRIGGER_FROMBUS    0x00
#define   MPS_INTI_TRIGGER_EDGE       0x04
#define   MPS_INTI_TRIGGER_LEVEL      0x0C

typedef struct IntSrcOverride {
  uint8_t   type;
  uint8_t   length;
  uint8_t   bus;		/* constant 0, meaning ISA */
  uint8_t   source;		/* Bus-relative IRQ */
  uint32_t  globalSystemInterrupt; /* what it got mapped to */
  uint16_t  flags;		   /* MPS INTI flags */
} IntSrcOverride;


/** @brief Populate the CPU structures for each of the available CPUs.
 *
 * Returns number of CPUs according to ACPI, or 0 if ACPI information
 * not available.  If successful, sets lapic_requires_8259_disable and
 * lapic_pa as a side effect.
 */
extern size_t acpi_probe_cpus(void);

/** @brief Probe the ACPI tables for IRQ sources and initialize vector
 * table if found.
 *
 * Return true if ACPI interrupt sources were found, in which case we
 * will not be using the 8259 PIC.
 */
extern bool acpi_probe_apics(void);

/** @brief Find out where ACPI mapped a given interrupt.
 *
 * If the passed irq is in the IBUS_GLOBAL namespace, find the
 * associated source. If the passed irq is in any other namespace,
 * find the appropriate target.
 */
extern bool acpi_map_interrupt(irq_t irq, IntSrcOverride *isovr);

#endif /* __I386_ACPI_H__ */
