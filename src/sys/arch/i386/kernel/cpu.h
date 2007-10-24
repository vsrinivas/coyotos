#ifndef __I386_CPU_H__
#define __I386_CPU_H__
/*
 * Copyright (C) 2007, The EROS Group, LLC
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
 * @brief CPU initialization
 */
#include <stdbool.h>
#include <hal/kerntypes.h>
#include <hal/config.h>
#include <kerninc/ccs.h>

/** @brief Probe the ACPI or MP table data, returning the number of
 * CPUs present on this machine.
 *
 * Updates cpu_ncpu as a side effect and returns that value.
 */
extern size_t cpu_probe_cpus(void);

extern void cpu_scan_features(void);

/**
 * @brief structure for return of cpuid information.
 *
 * The registers in this structure are in an unusual order for a
 * reason. Check how the vendor ID comparison is being done within
 * cpu_scan_features().
 */
typedef struct cpuid_regs_t {
  uint32_t eax;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
} cpuid_regs_t;

extern uint32_t cpuid(uint32_t code, cpuid_regs_t *regs);


typedef struct ArchCPU {
  kpa_t     localDataPageTable;
  kpa_t     transMapMappingPage;

  uint8_t   lapic_id;
} ArchCPU;

extern ArchCPU archcpu_vec[MAX_NCPU];

#endif /* __I386_CPU_H__ */
