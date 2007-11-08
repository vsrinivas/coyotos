#ifndef __HAL_ATOMIC_H__
#define __HAL_ATOMIC_H__
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

#include <inttypes.h>

#include <target-hal/atomic.h>

/** @file
 *
 * @brief Structure declarations for atomic words and their
 * manipulators. 
 *
 * Note that the compare_and_swap variants must NOT be used to guard
 * interactions between interrupt-level code and kernel-level
 * code. This is important because some targets (e.g. Coldfire) do not
 * provide any means to implement a truly atomic compare and swap
 * efficiently. If we find that it becomes necessary to use compare
 * and swap for this purpose we will need to disable interrupts on
 * those platforms.
 */

typedef TARGET_HAL_ATOMIC32_T Atomic32_t;
typedef TARGET_HAL_ATOMICPTR_T AtomicPtr_t;

/** @brief Atomic compare and swap.
 *
 * If current word value is @p oldval, replace with @p
 * newval. Regardless, return value of target word prior to
 * operation. */
static inline uint32_t compare_and_swap(Atomic32_t *, uint32_t oldval, uint32_t newval);
/** @brief Atomic compare and swap pointer */
static inline void *   compare_and_swap_ptr(AtomicPtr_t *, void* oldval, void * newval);

#define CAS_PTR(T, pVal, oldVal, newVal) \
  ((T) compare_and_swap_ptr(pVal, oldVal, newVal))

/** @brief Atomic word read. */
static inline uint32_t atomic_read(Atomic32_t *a);
/** @brief Atomic word write. */
static inline void atomic_write(Atomic32_t *a, uint32_t u);

/** @brief Atomic pointer read. */
static inline void * atomic_read_ptr(AtomicPtr_t *a);
/** @brief Atomic pointer write. */
static inline void atomic_write_ptr(AtomicPtr_t *a, void *vp);

/** @brief Atomic set bits into word. */
static inline void atomic_set_bits(Atomic32_t *a, uint32_t mask);
/** @brief Atomic clear bits into word. */
static inline void atomic_clear_bits(Atomic32_t *a, uint32_t mask);

#endif /* __HAL_ATOMIC_H__ */
