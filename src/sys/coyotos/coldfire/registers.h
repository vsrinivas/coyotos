#ifndef COLDFIRE_REGISTERS_H
#define COLDFIRE_REGISTERS_H
/*
 * Copyright (C) 2007, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
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
 * @brief Register structure type definitions.
 *
 * This is moderately awkward, because we really want IDL to own the
 * definitions, but IDL isn't all that gracious about it...
 */

/** @brief Architecture-specific fixed-point registers layout */
typedef struct coldfire_fixregs_t {
  uint32_t pc;
} coldfire_fixregs_t;

/** @brief Architecture-specific floating point and SIMD registers layout.
 */ 
typedef struct coldfire_floatregs_t {
  uint32_t        someday;
} coldfire_floatregs_t;

typedef struct coldfire_regset_t {
  coldfire_fixregs_t   fix;
  coldfire_floatregs_t fp;
} coldfire_regset_t;

#if (COYOTOS_TARGET == COYOTOS_TARGET_coldfire)
typedef coldfire_regset_t  regset_t;
typedef coldfire_fixregs_t fixregs_t;
#endif

#endif /* COLDFIRE_REGISTERS_H */
