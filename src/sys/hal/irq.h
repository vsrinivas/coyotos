#ifndef HAL_IRQ_H
#define HAL_IRQ_H
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
 * @brief Machine-specific global constants for interrupt handling.
 */

#include <target-hal/irq.h>

/** @brief Number of hardware-defined interrupt entries */
#define NUM_IRQ      TARGET_HAL_NUM_IRQ

/** @brief Number of hardware-defined trap (exception) entries */
#define NUM_TRAP     TARGET_HAL_NUM_TRAP

/** @brief Number of hardware-defined vectors */
#define NUM_VECTOR   TARGET_HAL_NUM_VECTOR

#ifndef __ASSEMBLER__

/** @brief Opaque flags type definition. 
 *
 * Target-specific HAL must define TARGET_FLAGS_T
*/
typedef TARGET_FLAGS_T flags_t;

/** @brief IRQ type. 
 *
 * Target-specific HAL must define TARGET_IRQ_T
*/
typedef TARGET_IRQ_T irq_t;

/** @brief Disable interrupts on the local processor (only).
 *
 * Using this in machine-independent code is a mistake. In most cases,
 * you should instead be using mutex_grab(). It exists in the HAL so
 * that it can be called in the implementation of printf().
 */
static inline flags_t locally_disable_interrupts();

/** @brief Enable interrupts on the local processor (only).
 *
 * Using this in machine-independent code is a mistake. In most cases,
 * you should instead be using mutex_grab(). It exists in the HAL so
 * that it can be called in the implementation of printf().
 */
static inline void locally_enable_interrupts(flags_t oldFlags);

/** @brief Return true IFF interrupts are enabled at the local processor.
 */
static inline bool local_interrupts_enabled();

#endif /* __ASSEMBLER__ */

#endif /* HAL_IRQ_H */
