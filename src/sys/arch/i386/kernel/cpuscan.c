/*
 * Copyright (C) 2005, The EROS Group, LLC.
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
 * @brief CPU feature scan support.
 */

#include <stddef.h>
#include <stdbool.h>

#include <hal/kerntypes.h>
#include <kerninc/ccs.h>
#include <kerninc/string.h>
#include <kerninc/printf.h>
#include <kerninc/util.h>
#include <kerninc/assert.h>

#include "cpu.h"
#include "IA32/CPUID.h"

uint32_t
cpuid(uint32_t code, cpuid_regs_t *regs)
{
  GNU_INLINE_ASM("movl %4,%%eax\n"
		 "cpuid\n"
		 "movl %%eax,%0\n"
		 "movl %%ebx,%1\n"
		 "movl %%ecx,%2\n"
		 "movl %%edx,%3\n"
		 : "=a" (regs->eax), "=b" (regs->ebx), 
		   "=c" (regs->ecx), "=d" (regs->edx)
		 : "a" (code));

  return regs->eax;
}

#define UNKNOWN 0x0u
#define INTEL   0x1u
#define AMD     0x2u
#define TMETA   0x4u

#define REG_EAX 0
#define REG_EBX 1
#define REG_ECX 2
#define REG_EDX 3

struct {
  uint32_t vendor;
  const char *idString;
} cpu_vendor[] = {
  /* Note that the following strings must be null padded to 12 bytes
     explicitly if they are shorter! */
  { UNKNOWN, "MysteryChips" },
  { INTEL,   "GenuineIntel" },
  { AMD,     "AuthenticAMD" },
  { TMETA,   "GenuineTMx86" },
};

#define CPUFEATURE(op, register, name, bit, vendor)	\
  { op, REG_##register, #name, bit, vendor }

struct {
  uint32_t opcode;
  uint32_t reg;
  const char *name;
  uint32_t bit;
  uint32_t vendors;
} features[] = {

  ///////////////////////////////////////////////////////
  // EAX=1, ECX feature mask
  ///////////////////////////////////////////////////////
  CPUFEATURE(0x00000001u, ECX, SSE3,       0,  INTEL|AMD),
  // bits 1,2 reserved
  CPUFEATURE(0x00000001u, ECX, MONITOR,    3,  INTEL),
  CPUFEATURE(0x00000001u, ECX, DSCPL,      4,  INTEL),
  CPUFEATURE(0x00000001u, ECX, VMX,        5,  INTEL),
  // bit 6 reserved
  CPUFEATURE(0x00000001u, ECX, EST,        7,  INTEL),
  CPUFEATURE(0x00000001u, ECX, TM2,        8,  INTEL),
  // bit 9 reserved
  CPUFEATURE(0x00000001u, ECX, CID,        10, INTEL),
  // bits 11,12 reserved
  CPUFEATURE(0x00000001u, ECX, CMPXCHG16,  13, INTEL|AMD),
  // bits 14-31 reserved

  ///////////////////////////////////////////////////////
  // EAX=1, EDX feature mask
  ///////////////////////////////////////////////////////
  CPUFEATURE(0x00000001u, EDX, FPU,        0,  INTEL|AMD|TMETA),
  CPUFEATURE(0x00000001u, EDX, VME,        1,  INTEL|AMD|TMETA),
  CPUFEATURE(0x00000001u, EDX, DE,         2,  INTEL|AMD|TMETA),
  CPUFEATURE(0x00000001u, EDX, PSE,        3,  INTEL|AMD|TMETA),
  CPUFEATURE(0x00000001u, EDX, TSC,        4,  INTEL|AMD|TMETA),
  CPUFEATURE(0x00000001u, EDX, MSR,        5,  INTEL|AMD|TMETA),
  CPUFEATURE(0x00000001u, EDX, PAE,        6,  INTEL|AMD),
  CPUFEATURE(0x00000001u, EDX, MCE,        7,  INTEL|AMD),
  CPUFEATURE(0x00000001u, EDX, CX8,        8,  INTEL|AMD|TMETA),
  CPUFEATURE(0x00000001u, EDX, APIC,       9,  INTEL|AMD),
  /* bit 10 reserved */
  CPUFEATURE(0x00000001u, EDX, SEP,        11, INTEL|AMD|TMETA),
  CPUFEATURE(0x00000001u, EDX, MTRR,       12, INTEL|AMD),
  CPUFEATURE(0x00000001u, EDX, PGE,        13, INTEL|AMD),
  CPUFEATURE(0x00000001u, EDX, MCA,        14, INTEL|AMD),
  CPUFEATURE(0x00000001u, EDX, CMOV,       15, INTEL|AMD|TMETA),
  CPUFEATURE(0x00000001u, EDX, PAT,        16, INTEL|AMD),
  CPUFEATURE(0x00000001u, EDX, PSE36,      17, INTEL|AMD),
  CPUFEATURE(0x00000001u, EDX, PSN,        18, INTEL|AMD|TMETA),
  CPUFEATURE(0x00000001u, EDX, CLFSH,      19, INTEL|AMD),
  /* bit 20 reserved */
  CPUFEATURE(0x00000001u, EDX, DS,         21, INTEL),
  CPUFEATURE(0x00000001u, EDX, ACPI,       22, INTEL),
  CPUFEATURE(0x00000001u, EDX, MMX,        23, INTEL|AMD|TMETA),
  CPUFEATURE(0x00000001u, EDX, FXSR,       24, INTEL|AMD),
  CPUFEATURE(0x00000001u, EDX, SSE,        25, INTEL|AMD),
  CPUFEATURE(0x00000001u, EDX, SSE2,       26, INTEL|AMD),
  CPUFEATURE(0x00000001u, EDX, SS,         27, INTEL),
  CPUFEATURE(0x00000001u, EDX, HT,         28, INTEL|AMD),
  CPUFEATURE(0x00000001u, EDX, TM,         29, INTEL),
  /* bit 30 reserved */
  CPUFEATURE(0x00000001u, EDX, SBF,        31, INTEL),

  ///////////////////////////////////////////////////////
  // EAX=0x80000001u, ECX feature mask
  ///////////////////////////////////////////////////////
  CPUFEATURE(0x80000001u, ECX, LAHF64,      0, INTEL),
  CPUFEATURE(0x80000001u, ECX, CMPLEGACY,   1, AMD),
  CPUFEATURE(0x80000001u, ECX, SVM,         2, AMD),
  CPUFEATURE(0x80000001u, ECX, AltMovCr8,   4, AMD),

  ///////////////////////////////////////////////////////
  // EAX=0x80000001u, EDX feature mask
  ///////////////////////////////////////////////////////
  CPUFEATURE(0x80000001u, EDX, SYSCALL,    11, INTEL|AMD),
  CPUFEATURE(0x80000001u, EDX, FCMOV,      16, TMETA),
  CPUFEATURE(0x80000001u, EDX, NX,         20, INTEL|AMD),
  CPUFEATURE(0x80000001u, EDX, MMXExt,     22, AMD),
  CPUFEATURE(0x80000001u, EDX, FFXSR,      25, AMD),
  CPUFEATURE(0x80000001u, EDX, RDTSCP,     27, AMD),
  CPUFEATURE(0x80000001u, EDX, LM,         29, AMD),
  CPUFEATURE(0x80000001u, EDX, EM64T,      29, INTEL),
  CPUFEATURE(0x80000001u, EDX, 3DNowExt,   30, AMD),
  CPUFEATURE(0x80000001u, EDX, 3DNow,      31, AMD),

#if 0
  // Following bits are AMD only and are redundant with eax=1,edx
  // and so are commented out of the list here.
  CPUFEATURE(0x80000001u, EDX, FPU,         0, AMD|TMETA),
  CPUFEATURE(0x80000001u, EDX, VME,         1, AMD|TMETA),
  CPUFEATURE(0x80000001u, EDX, DE,          2, AMD|TMETA),
  CPUFEATURE(0x80000001u, EDX, PSE,         3, AMD|TMETA),
  CPUFEATURE(0x80000001u, EDX, TSC,         4, AMD|TMETA),
  CPUFEATURE(0x80000001u, EDX, MSR,         5, AMD|TMETA),
  CPUFEATURE(0x80000001u, EDX, PAE,         6, AMD),
  CPUFEATURE(0x80000001u, EDX, MCE,         7, AMD),
  CPUFEATURE(0x80000001u, EDX, CX8,         8, AMD|TMETA),
  CPUFEATURE(0x80000001u, EDX, APIC,        9, AMD),
  CPUFEATURE(0x80000001u, EDX, MTRR,       12, AMD),
  CPUFEATURE(0x80000001u, EDX, PGE,        13, AMD),
  CPUFEATURE(0x80000001u, EDX, MCA,        14, AMD),
  CPUFEATURE(0x80000001u, EDX, CMOV,       15, AMD|TMETA),
  CPUFEATURE(0x80000001u, EDX, PAT,        16, AMD),
  CPUFEATURE(0x80000001u, EDX, PSE36,      17, AMD),
  CPUFEATURE(0x80000001u, EDX, MMX,        23, AMD|TMETA),
  CPUFEATURE(0x80000001u, EDX, FXSR,       24, AMD),
#endif

  ///////////////////////////////////////////////////////
  // EAX=0x80000007u, EDX feature mask
  ///////////////////////////////////////////////////////

  // STC: software thermal control
  CPUFEATURE(0x80000007u, EDX, TS,          0, AMD),
  CPUFEATURE(0x80000007u, EDX, FID,         1, AMD),
  CPUFEATURE(0x80000007u, EDX, VID,         2, AMD),
  CPUFEATURE(0x80000007u, EDX, TIP,         3, AMD),
  CPUFEATURE(0x80000007u, EDX, TM,          4, AMD),
  CPUFEATURE(0x80000007u, EDX, STC,         5, AMD),

  // Following are bits supported by AMD that are redundant, and so
  // not reported from the kernel:
  { ~0u }
};

/**
 * @brief Scan CPUs, displaying feature information.
 *
 * At the moment, essentially all this does is features for CPU0.
 * Note that we didn't make it past the bootstrap code without
 * confirming that the CPUID instruction works,
 *
 * Strategy: use CPUID eax=0 to obtain the vendor id, and CPUID eax=1
 * to obtain the model and stepping information (to work around bugs).
 *
 * Given these, proceed through the legal CPUID eax values making the
 * calls, and for each such call iterate over the feature vector and
 * print the relevant feature information. This isn't as efficient as
 * it could be, but we are only going to do this once, and it makes
 * updating the feature vector much much easier.
 */
void
cpu_scan_features(void)
{
  const size_t nVendor = sizeof(cpu_vendor) / sizeof(cpu_vendor[0]);
  uint32_t vendor = UNKNOWN;
  cpuid_regs_t regs;

  // CPUID eax=0 to find vendor ID and max legal eax value:
  uint32_t low_max = cpuid(0, &regs);
  const char *vendor_id = (const char *) &regs.ebx;
  for (size_t i = 0; i < nVendor; i++) {
    if (memcmp(vendor_id, cpu_vendor[i].idString, 12) == 0) {
      vendor = i;
      break;
    }
  }

  printf("CPU0: %s\n", cpu_vendor[vendor].idString);

  // All processors that survive low boot support at least op 1.
  assert(low_max > 0);
  // CPUID eax=1 returns model and stepping information.
  uint32_t cpuSignature = cpuid(1u, &regs);

  if (vendor & (INTEL|AMD)) {
    uint32_t famid = FIELD(regs.eax, 11, 8);
    uint32_t model = FIELD(regs.eax, 7, 4);
#if HAVE_HUI
    uint32_t stepping = FIELD(regs.eax, 3, 0);
#endif

    if (famid == 0xf)
      famid += FIELD(regs.eax, 27, 20);
    if ((famid == 0xf) || ((vendor == INTEL) && (famid == 0x6)))
      model += (FIELD(regs.eax, 19, 16) << 4);

    uint32_t nHyperThreads = FIELD(regs.ebx, 23, 16);
    if ((regs.edx & CPUID_EDX_HT) == 0)
      nHyperThreads = 1;

    printf("CPU0: family 0x%x model 0x%x stepping 0x%x brand %d clflush %d\n", 
	   famid, model, stepping,
	   FIELD(regs.ebx, 7, 0),
	   FIELD(regs.ebx, 15, 8));
    printf("CPU0: Logical CPUs %d Init APIC ID 0x%x\n", 
	   nHyperThreads,
	   FIELD(regs.ebx, 31, 24));
  }

  if (vendor == AMD) {
    uint32_t famid = FIELD(regs.eax, 11, 8);
    uint32_t model = FIELD(regs.eax, 7, 4);
#if HAVE_HUI
    uint32_t stepping = FIELD(regs.eax, 3, 0);
#endif

    if (famid == 0xf)
      famid += FIELD(regs.eax, 27, 20);
    if (famid == 0x6 || famid == 0xf)
      model += (FIELD(regs.eax, 19, 16) << 4);

    printf("CPU0: family 0x%x model 0x%x stepping 0x%x brand %d clflush %d\n", 
	   famid, model, stepping,
	   FIELD(regs.ebx, 7, 0),
	   FIELD(regs.ebx, 15, 8));
    printf("CPU0: Max CPU %d Init APIC ID %d\n", 
	   FIELD(regs.ebx, 23, 16),
	   FIELD(regs.ebx, 31, 24));
  }

  uint32_t count = 0;

  for (uint32_t opcode = 0x1u; opcode < low_max; opcode++) {
    cpuid(opcode, &regs);

    for (size_t i = 0; i < (sizeof(features) / sizeof(features[0])); i++) {
      bool show = false;

      if (features[i].opcode == opcode && (features[i].vendors & vendor)) {
	if ((features[i].reg == REG_EAX) && 
	    (regs.eax & (1u << features[i].bit)))
	  show = true;
	else if ((features[i].reg == REG_EBX) && 
	    (regs.ebx & (1u << features[i].bit)))
	  show = true;
	else if ((features[i].reg == REG_ECX) && 
	    (regs.ecx & (1u << features[i].bit)))
	  show = true;
	else if ((features[i].reg == REG_EDX) && 
	    (regs.edx & (1u << features[i].bit)))
	  show = true;

	// ERRATA: Intel screwed up the SEP implementation on early
	// Pentium Pro's, so we need to check the CPU signature in
	// that case.
	if ((features[i].opcode == 0x1) &&
	    (features[i].reg == REG_EDX) &&
	    (features[i].bit == 11) &&
	    ((cpuSignature & 0x0fff3fffu) < 0x633u))
	  show = false;

	if (show) {
	  if ((count % 16) == 0) {
	    if (count) printf("\n");
	    printf("CPU0:");
	  }

	  printf(" %s", features[i].name);
	  count++;
	}
      }
    }
  }

  // Note: contrary to documentation, hi_max may come back with a
  // value BELOW 0x80000000u, in which case the upper values are not
  // supported.
  uint32_t hi_max = cpuid(0x80000000u, &regs);

  for (uint32_t opcode = 0x80000000u; opcode < hi_max; opcode++) {
    cpuid(opcode, &regs);

    for (size_t i = 0; i < (sizeof(features) / sizeof(features[0])); i++) {
      bool show = false;

      if (features[i].opcode == opcode && (features[i].vendors & vendor)) {
	if ((features[i].reg == REG_ECX) && 
	    (regs.ecx & (1u << features[i].bit)))
	  show = true;
	else if ((features[i].reg == REG_EDX) && 
	    (regs.edx & (1u << features[i].bit)))
	  show = true;

	// ERRATA: Intel screwed up the SEP implementation on early
	// Pentium Pro's, so we need to check the CPU signature in
	// that case.
	if ((features[i].opcode == 0x1) &&
	    (features[i].reg == REG_EDX) &&
	    (features[i].bit == 11) &&
	    ((cpuSignature & 0x0fff3fffu) < 0x633u))
	  show = false;

	if (show) {
	  if ((count % 16) == 0) {
	    if (count) printf("\n");
	    printf("CPU0:");
	  }

	  printf(" %s", features[i].name);
	  count++;
	}
      }
    }
  }

  if (count) printf("\n");
}

