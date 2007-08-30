#ifndef __KERNINC_MUTEX_H__
#define __KERNINC_MUTEX_H__
/*
 * Copyright (C) 2007, The EROS Group, LLC
 *
 * This file is part of the Coyotos Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Mutual Exclusion primitives.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <hal/atomic.h>
#include "CPU.h"

/** 
 * @brief lock type for cpu-owned lock.
 *
 * A cross-trap is needed to acquire it.
 */
#define LTY_CPU  0x1u
/** @brief lock type for per-transaction lock */
#define LTY_TRAN 0x2u

#define LOCKVALUE(gen,ty,cpu) \
  (((gen) * 4 * MAX_NCPU) | ((ty) * MAX_NCPU) | (cpu))

#define LOCK_TYPE(v) (((v) & (3 * MAX_NCPU))/MAX_NCPU)
#define LOCK_CPU(v) ((v) & (MAX_NCPU - 1))
#define LOCK_GENERATION(v) ((v) / (4 * MAX_NCPU))

#define LOCK_INCGEN(v) (v + (4 * MAX_NCPU))
/**
 * @brief Mutex type.
 *
 * A held lock ensures both mutual exclusion and residency.  Mutexes
 * can be acquired recursively. They can be selectively released one
 * at a time for hot cases, or they can be gang-released at the end of
 * the kernel operation.
 */
typedef struct mutex_t {
  Atomic32_t  _opaque;
} mutex_t;

#define MUTEX_INIT { { 0 } }

/**
 * @brief Held lock information, needed to release the lock.
 */
typedef struct HoldInfo {
  mutex_t *lockPtr;
  uint32_t oldValue;
} HoldInfo;

/**
 * @brief Grab @p mtx, returning a HoldInfo structure for its release.
 *
 * May YIELD() if attempting to grab fails and we have been asked to
 * back off by a higher priority process.
 */
HoldInfo mutex_grab(mutex_t *mtx);

/**
 * @brief Grab @p mtx unconditionally on behalf of the current CPU,
 * returning a HoldInfo structure for its release.
 *
 * This is used in the scheduler when locking the newly selected
 * current process to the current CPU.
 */
HoldInfo mutex_cpulock(mutex_t *mtx);

/**
 * @brief Attempt to grab @p mtx.  On failure, returns false.  On success,
 * @p hi is filled in with the release information, and the call returns true.
 */
bool mutex_trygrab(mutex_t *mtx, HoldInfo *hi);

/**
 * @brief Release a held mutex.
 *
 * Mutexes should be released in a strict stack fashion; the argument
 * must be from the most recent mutex_grab() or successful
 * mutex_trygrab() which has not yet been released.
 *
 * @bug Shap believes that stack order is desirable but not required,
 * because we are now doing deadlock avoidance and we have instances
 * where this deacquisition policy must not be followed.
 */
void mutex_release(HoldInfo hi);

/** @brief Return true if this mutex is held by the inquiring CPU.
 */
bool mutex_isheld(mutex_t *mtx);

#if 0
/**
 * @brief Grab a new lock after dropping the first one, as a single operation.
 *
 * @p hi must be the most recent lock acquired.
 */
HoldInfo mutex_handoff(HoldInfo hi, mutex_t *mtx);
#endif

/** @brief Release all process locks currently held by the process
 * that is running on this CPU. */
inline static void mutex_release_all_process_locks()
{
  MY_CPU(curCPU)->procMutexValue = 
    LOCK_INCGEN(MY_CPU(curCPU)->procMutexValue);
}

/**
 * @brief Spinlock type.
 *
 * A spinlock is actually a mutex, but one that should always be
 * acquired unconditionally. In contrast to mutex acquisition,
 * acquisition of a spinlock will not yield, and spinlocks MUST be
 * explicitly released.
 *
 * IT IS AN ERROR to call any procedure that might yield() while
 * holding a spinlock.
 *
 * We use a different type to help catch misuse errors and facilitate
 * static analysis.
 */
typedef struct spinlock_t {
  mutex_t m;
} spinlock_t;

#define SPINLOCK_INIT { MUTEX_INIT }

/** @brief Encapsulation of held spinlock.
 *
 * We use a different type to help catch misuse errors and facilitate
 * static analysis.
 */
typedef struct SpinHoldInfo {
  HoldInfo hi;
} SpinHoldInfo;

/**
 * @brief Grab @p spl unconditionally, returning a HoldInfo structure
 * for its release.
 *
 * Will not yield to a higher priority process. This should only be
 * used within critical sections, when you know that no yield is
 * possible. Spinlocks may @em not be recursivelyacquired.
 */
SpinHoldInfo spinlock_grab(spinlock_t *spl);

/**
 * @brief Release a held spinlock.
 *
 * Spinlocks must be released in a strict stack fashion; the argument
 * must be from the most recent mutex_grab() or successful
 * mutex_trygrab() which has not yet been released.
 */
static inline void spinlock_release(SpinHoldInfo shi)
{
  mutex_release(shi.hi);
}

/** @brief Return true if this spinlock is held by the current CPU/Process.
 */
static inline bool spinlock_isheld(spinlock_t *spl)
{
  return mutex_isheld(&spl->m);
}

#if 0
typedef struct CritSection {
  HoldInfo hi;
  flags_t  flags;
} CritSection;

/** @brief Start a cross-CPU criticical section.
 *
 * This disable interrupts locally and then spins for a critical
 * section lock. This ensures that neither local nor foreign
 * concurrency can intervene with this critical region.  There is only
 * ONE critical section lock (globally).
 * 
 * No further locks should be taken from within the
 * critical section.
 *
 * Realistically, the only code that should be calling this is
 * printf(), and that only because we may want to call printf() from
 * within debugging code.
 */
CritSection BEGIN_CRITICAL_SECTION();

/** @brief End a criticical section. */
void END_CRITICAL_SECTION(CritSection);
#endif

#endif /* __KERNINC_MUTEX_H__ */
