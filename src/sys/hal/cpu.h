#ifndef HAL_CPU_H
#define HAL_CPU_H
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

#include <hal/kerntypes.h>
#include <target-hal/cpu.h>

/** @file
 * @brief Mechanism to rapidly get the current CPU structure pointer.
 */

struct CPU;

/** @brief Return the CPUID (index into cpu vector) of the currently
 * executing processor.
 *
 * This is NOT the preferred way to do this, as it is fairly
 * expensive.
 */
__hal cpuid_t cpu_getMyID();

/** @brief Return pointer to CPU structure corresponding to currently
 * executing CPU.
 *
 * Note that the "const" declaration is a blatant lie, because all
 * real implementations of this procedure access memory. However, the
 * relevant memory location is never modified once it is established,
 * so we need not consider writes that might proceed through aliases.
 */
__hal static inline struct CPU *current_cpu() __attribute__((always_inline, pure));


#endif /* HAL_VM_KMAP_H */
