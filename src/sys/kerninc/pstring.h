#ifndef __KERNINC_PSTRING_H__
#define __KERNINC_PSTRING_H__
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
 * @brief Physical memory-related functions
 */
#include <stddef.h>

#include <coyotos/coytypes.h>
#include <hal/transmap.h>

extern void memcpy_vtop(kpa_t dest_pa, void *source_va, size_t length);
extern void memcpy_ptov(void *dest_va, kpa_t source_pa, size_t length);
extern void memcpy_ptop(kpa_t dest_pa, kpa_t source_pa, size_t length);
extern void memset_p(kpa_t pa, int value, size_t length);

#endif /* __KERNINC_PSTRING_H__ */
