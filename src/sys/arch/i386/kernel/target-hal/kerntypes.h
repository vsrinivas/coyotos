#ifndef I386_HAL_KERNTYPES_H
#define I386_HAL_KERNTYPES_H
/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the EROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Definition of kernel datatypes.
 */

#include <hal/config.h>

/** @brief Integral type sufficient to hold a kernel-implemented
 * physical address.
 */
#define TARGET_HAL_KPA_T uint64_t

/** @brief Integral type sufficient to hold a count of items that will
 * fit in physical memory.
 *
 * See PhysMem.h. We need this mainly because of IA-32.
 */
#define TARGET_HAL_KPSIZE_T uint64_t

/** @brief Integral type sufficient to hold a kernel virtual address. */
#define TARGET_HAL_KVA_T uint32_t

/** @brief Integral type sufficient to hold a user virtual address. */
#define TARGET_HAL_UVA_T uint32_t

/* Following does not appear in most other architectures, but is
   needed for Pentium */

#endif /* I386_HAL_KERNTYPES_H */
