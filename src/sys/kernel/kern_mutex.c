/*
 * Copyright (C) 2007, The EROS Group, LLC.
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
 * @brief Kernel Mutual Exclusion Primitives
 */

#include <coyotos/coytypes.h>
#include <kerninc/assert.h>
#include <kerninc/printf.h>
#include <kerninc/mutex.h>
#include <kerninc/CPU.h>
#include <kerninc/Sched.h>
#include <stdbool.h>

/**
 * @brief Attempt to grab @p mtx once.
 *
 * Attempt to grab @p mtx, which has a current value of oldval.  
 *
 * If the lock cannot be grabbed, returns false, and @p outval is unchanged.
 *
 * If the lock can be grabbed, returns true, and @p outval is updated to be
 * the value the lock should take upon release.
 */
static bool
mutex_do_trylock(mutex_t *mtx, uint32_t curval, uint32_t *outval)
{
  uint32_t cas_against = 0;

  switch (LOCK_TYPE(curval)) {
  case LTY_TRAN: {
    size_t cpuidx = LOCK_CPU(curval);
    assert(cpuidx < cpu_ncpu);
    
    CPU *cpu = &cpu_vec[cpuidx];
    if (curval != cpu->procMutexValue) {
      // we can take the lock directly; it's been gang-released
      cas_against = curval;
      break;
    }

    if (cpu == MY_CPU(curCPU)) {
      // valid lock held by our CPU;  allow the recursive lock
      *outval = curval;
      return true;
    }
    break;
  }

  default:
    assert(curval == 0);
    break;
  }

  uint32_t cur = 
    compare_and_swap(&mtx->_opaque, cas_against, 
		     MY_CPU(curCPU)->procMutexValue);

  *outval = cur;
  return (cur == cas_against);
}

bool
mutex_isheld(mutex_t *mtx)
{
  uint32_t curval = atomic_read(&mtx->_opaque);
  return (curval == MY_CPU(curCPU)->procMutexValue);
}

HoldInfo
mutex_grab(mutex_t *mtx)
{
  uint32_t oldval = atomic_read(&mtx->_opaque);
  for (;;) {
    if (mutex_do_trylock(mtx, oldval, &oldval)) {
      HoldInfo hi = {mtx, oldval};
      return hi;
    }

    if (MY_CPU(curCPU)->shouldDefer)
      sched_abandon_transaction();
  }
}

bool
mutex_trygrab(mutex_t *mtx, HoldInfo *hi)
{
  uint32_t oldval = atomic_read(&mtx->_opaque);

  if (mutex_do_trylock(mtx, oldval, &oldval)) {
    if (hi != NULL) {
      hi->lockPtr = mtx;
      hi->oldValue = oldval;
    }
    return true;
  }
  return false;
}

void
mutex_release(HoldInfo hi)
{
  uint32_t curVal = atomic_read(&hi.lockPtr->_opaque);

#ifndef NDEBUG
  if (curVal != MY_CPU(curCPU)->procMutexValue) {
    fatal("mutex_release: cpu %d, %08lx: is gen=%d:ty=%d:cpu=%d,\n"
	  "expected gen=%d:ty=%d:cpu=%d\n",
	  MY_CPU(curCPU)->id, hi.lockPtr, 
	  LOCK_GENERATION(curVal),
	  LOCK_TYPE(curVal),
	  LOCK_CPU(curVal),
	  LOCK_GENERATION(MY_CPU(curCPU)->procMutexValue),
	  LOCK_TYPE(MY_CPU(curCPU)->procMutexValue),
	  LOCK_CPU(MY_CPU(curCPU)->procMutexValue));
  }
#endif
  atomic_write(&hi.lockPtr->_opaque, hi.oldValue);
}

#if 0
HoldInfo
mutex_handoff(HoldInfo ohi, mutex_t *mtx)
{
  if (ohi.lockPtr == mtx)
    return ohi;

  HoldInfo hi = mutex_grab(mtx);
  mutex_release(ohi);

  return hi;
}
#endif

SpinHoldInfo
spinlock_grab(spinlock_t *spl)
{
  uint32_t oldval = atomic_read(&spl->m._opaque);
  for (;;) {
    if (mutex_do_trylock(&spl->m, oldval, &oldval)) {
      SpinHoldInfo shi = { {&spl->m, oldval} };

      return shi;
    }
  }
}
