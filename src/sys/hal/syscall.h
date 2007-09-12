#ifndef HAL_SYSCALL_H
#define HAL_SYSCALL_H
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
 * @brief Machine-dependent system call support.
 *
 * These are the procedures that know how to get/set the various
 * system call parameters.
 */

#include <target-hal/syscall.h>

/** @brief Return TRUE if the non-registerized parameter block falls
 * within valid user-mode addresses according to the rules of the
 * current architecture.
 */
static inline bool valid_param_block(Process *p);

/** @brief Save the necessary architecture-dependent parameter words
 * that are not transferred across the system call boundary in
 * registers.
 *
 * Note that this need not save parameter words that can later be
 * fetched from sender memory.
 */
static inline void save_soft_parameters(Process *p);

/** @brief Copy received soft params back to user.
 *
 * This is @em always executed in the current address space.
 */
static inline void copyout_soft_parameters(Process *p);

/** @brief Fetch input parameter word @p pw from the appropriate
 * architecture-dependent location. */
static inline uintptr_t get_pw(Process *p, size_t pw);

/** @brief Fetch receive parameter word @p pw from the appropriate
 * architecture-dependent location. */
static inline uintptr_t get_rcv_pw(Process *p, size_t pw);

/** @brief Store input parameter word @p pw to the appropriate
 * architecture-dependent INPUT location. Used in transition to
 * receive phase. */
static inline void set_pw(Process *p, size_t pw, uintptr_t value);

/** @brief Set receiver epID to incoming epID value. */
static inline void set_epID(Process *p, uint64_t epID);

/** @brief Fetch receiver epID for matching. */
static inline uint64_t get_rcv_epID(Process *p);

#endif /* HAL_SYSCALL_H */
