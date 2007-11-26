#ifndef HAL_KERNTYPES_H
#define HAL_KERNTYPES_H
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
 * @brief Definition of kernel datatypes.
 */
#include <inttypes.h>

#include <target-hal/kerntypes.h>

#if MAX_NCPU <= 256
typedef uint8_t cpuid_t;
#else
typedef uint32_t cpuid_t;
#endif

/** @brief Integral type sufficient to hold a kernel-implemented
 * physical address.
 */
typedef TARGET_HAL_KPA_T kpa_t;

/** @brief Integral type sufficient to hold a count of items that will
 * fit in physical memory.
 *
 * See PhysMem.h. We need this mainly because of IA-32.
 */
typedef TARGET_HAL_KPSIZE_T kpsize_t;

/** @brief Value used to signal allocation failure in physical
    allocation */
#define BAD_KPA (~((kpa_t) 0))

/** @brief Integral type sufficient to hold a kernel virtual address. */
typedef TARGET_HAL_KVA_T kva_t;

/** @brief Integral type sufficient to hold a user virtual address. */
typedef TARGET_HAL_UVA_T uva_t;

/** @brief Integral type sufficient to hold transmap meta-map entries. */
typedef TARGET_TRANSMETA_T transmeta_t;

#ifdef TARGET_ASID_T
typedef TARGET_ASID_T asid_t;
#endif

#endif /* HAL_KERNTYPES_H */
