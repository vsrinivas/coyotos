#ifndef __IA32_CPUID_H__
#define __IA32_CPUID_H__
/*
 * Copyright (C) 2005, Jonathan S. Shapiro.
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
 * @brief CPUID feature flags for function 1, ECX/EDX.
 *
 * A more sophisticated decoding scheme is need to grab features in 
 * general. The definitions here are intended only to support
 * low-level bootstrap.
 */

/** @brief SSE3 supported. */
#define CPUID_EDX_SSE3            0x00000001
/** @brief 16 byte cmpxchg */
#define CPUID_ECX_CMPXCHG16B      0x00002000

/** @brief Monitor/mwait (Intel only) */
#define CPUID_ECX_INTEL_MONITOR   0x00000004
/** @brief DS-CPL: CPL-qualified debug store */
#define CPUID_ECX_INTEL_DSCPL     0x00000010
/** @brief virtual machine extensions (Intel only) */
#define CPUID_ECX_INTEL_VMX       0x00000020
/* NOTE: Following two items may be swapped -- manual disagrees with
   CPUID app note! */
/** @brief Enhanced speed step (Intel only) */
#define CPUID_ECX_INTEL_EST       0x00000080
/** @brief Thermal monitor 2 (Intel only) */
#define CPUID_ECX_INTEL_TM2       0x00000100
/** @brief context ID (Intel only) */
#define CPUID_ECX_INTEL_CID       0x00000400


/** @brief FPU on chip, 387 or better */
#define CPUID_EDX_FPU         0x00000001
/** @brief virtual mode extension */
#define CPUID_EDX_VME         0x00000002
/** @brief debugging extension */
#define CPUID_EDX_DE          0x00000004
/** @brief 4M page size extension */
#define CPUID_EDX_PSE         0x00000008
/** @brief time stamp counter present and controlled by CR4.TSD */
#define CPUID_EDX_TSC         0x00000010
/** @brief model specific registers using RDMSR/WRMSR */
#define CPUID_EDX_MSR         0x00000020
/** @brief PAE mode is supported */
#define CPUID_EDX_PAE         0x00000040
/** @brief machine check exception, exception 18, and CR4.MCE bits are
    supported. */
#define CPUID_EDX_MCE         0x00000080
/** @brief CMPXCHG8 instruction is supported. */
#define CPUID_EDX_CX8         0x00000100
/** @brief On-board APIC is present. */
#define CPUID_EDX_APIC        0x00000200
/* RESERVED EDX: 0x00000400 */
/** @brief Fast system call supported via SYSENTER/SYSEXIT */
#define CPUID_EDX_SEP         0x00000800
/** @brief Memory type range registers */
#define CPUID_EDX_MTRR        0x00001000
/** @brief page global extension */
#define CPUID_EDX_PGE         0x00002000
/** @brief machine check architecture */
#define CPUID_EDX_MCA         0x00004000
/** @brief conditional move is present */
#define CPUID_EDX_CMOV        0x00008000
/** @brief page attribute table */
#define CPUID_EDX_PAT         0x00010000
/** @brief 36-bit page size extension */
#define CPUID_EDX_PSE36       0x00020000
/** @brief processor serial number present and enabled. (Intel Only */
#define CPUID_EDX_INTEL_PSN   0x00040000
/** @brief Cache line flush instruction supported. */
#define CPUID_EDX_CLFSH       0x00080000
/* RESERVED EDX: 0x00100000 */
/** @brief debug store (Intel Only */
#define CPUID_EDX_INTEL_DS    0x00200000
/** @brief thermal monitor and SW clock features. (Intel only) */
#define CPUID_EDX_INTEL_ACPI  0x00400000
/** @brief MMX supported. */
#define CPUID_EDX_MMX         0x00800000
/** @brief Fast floating point save/restore. */
#define CPUID_EDX_FXSR        0x01000000
/** @brief SSE supported. */
#define CPUID_EDX_SSE         0x02000000
/** @brief SSE2 supported. */
#define CPUID_EDX_SSE2        0x04000000
/** @brief self-snoop. (Intel only) */
#define CPUID_EDX_INTEL_SS    0x08000000
/** @brief hyper-threading */
#define CPUID_EDX_HT          0x10000000
/** @brief thermal monitoring (Intel Only */
#define CPUID_EDX_INTEL_TM    0x20000000
/* RESERVED EDX: 0x40000000 */
/** @brief Signal break on FERR (Intel only) */
#define CPUID_EDX_INTEL_SBF   0x80000000

#endif /* __IA32_CPUID_H__ */
