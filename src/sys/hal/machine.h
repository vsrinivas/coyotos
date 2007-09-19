#ifndef HAL_MACHINE_H
#define HAL_MACHINE_H
/*
 * Copyright (C) 2005, The EROS Group, LLC.
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
 * @brief Miscellaneous machine-specific functions.
 */

#include <stdbool.h>
#include <target-hal/machine.h>

/** @brief Select a (possibly new) process to run.
 *
 * This procedure, when called, performs a non-local return into the
 * scheduler. The implementation is therefore part of the HAL layer.
 */
void sched_low_level_yield() NORETURN;

/** @brief Idle the current processor.
 *
 * This procedure, when called, puts the current processor into its
 * "idle" or "low power" mode. It is called from the scheduler when
 * there is nothing better to do. The implementation is necessarily
 * machine specific.
 */
void IdleThisProcessor() NORETURN;

/** @brief Halt the processor without rebooting.
 *
 * Implementation is architecture specific.
 */
extern void sysctl_halt(void) NORETURN;

/** @brief Power down the system if possible.
 *
 * Implementation is architecture specific.
 */
extern void sysctl_powerdown(void) NORETURN;

/** @brief Perform an orderly shutdown and reboot the system if possible.
 *
 * Implementation is architecture specific.
 */
extern void sysctl_reboot(void) NORETURN;

/** @brief Perform architecture-specific initialization. */
void arch_init();

/** @brief Perform architecture-specific cache initialization. */
void arch_cache_init();

struct ObjectHeader;

/** @brief Set up an object to discover if it was referenced.
 *
 * Called when an object is moved from the active aging pool to the
 * check aging pool. Tells the arch-dependent code that it should take
 * any necessary steps so that it will later be able to detect whether
 * this object was in fact referenced.
 */
void object_begin_refcheck(struct ObjectHeader *);

/** @brief Check later to see if the object was actually used. 
 *
 * Called when an object is moved from the active aging pool to the
 * check-before-reclaim aging pool. This is called just before we move
 * an object from the check aging pool to the reclaim aging pool. If
 * the object was referenced, we will instead move it back into the
 * active aging pool.
*/
bool object_was_referenced(struct ObjectHeader *);

#endif /* HAL_MACHINE_H */

