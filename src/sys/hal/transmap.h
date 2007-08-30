#ifndef HAL_TRANSMAP_H
#define HAL_TRANSMAP_H
/*
 * Copyright (C) 2006, The EROS Group, LLC.
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

#include <hal/kerntypes.h>

/** @file
 * @brief CPU-local transient mappings.
 */

/* *****************************************************************
 *
 * INTERFACES FOR KERNEL MAP MANAGEMENT
 *
 * *****************************************************************/

/** @brief Establish a temporary mapping on the current CPU for the
 * given physical address.
 *
 * At most 32 such mappings can be simultaneously live. The mapping
 * logic endeavours to avoid mappings that have previously been used,
 * in order to avoid unnecessary TLB flush.
 */
kva_t transmap_map(kpa_t pa);

#define TRANSMAP_MAP(pa, ty) ((ty) transmap_map(pa))

/** @brief Release a transient mapping.
 *
 * This marks the corresponding PTE invalid and <em>logically</em>
 * releases the mapping, but the TLB is flushed lazily. The general
 * idea is that if you use fewer than 32 transient mappings, you won't
 * need to flush the TLB at all.
 */
void transmap_unmap(kva_t va);

#define TRANSMAP_UNMAP(va) transmap_unmap((kva_t) va)

/** @brief Release all transient mappings, as when exiting to an
 * application address space under conditions where the TLB is
 * getting flushed anyway.
 *
 * This function does NOT flush the TLB! It is a book-keeping function
 * used to update the allocation bitmap used by the transmap
 * implementation. In theory, this shouldn't be necessary at all,
 * since the transient mappings should have been explicitly released
 * by the path before yielding.
 */
void transmap_flush();


#endif /* HAL_TRANSMAP_H */
