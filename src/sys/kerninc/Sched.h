#ifndef __KERNINC_SCHED_H__
#define __KERNINC_SCHED_H__
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
 * @brief Interface to the kernel scheduler. */

#include <kerninc/ccs.h>

/** @brief Abandon kernel path and find something useful to do.
 *
 * Causes the current kernel path to be abandoned by forcibly
 * clobbering the stack and entering the dispach path. This releases
 * all locks and pins, then calls sched_low_level_yield.
 *
 * The value of @tt current does not change. Depending on
 * circumstances, the current process may be able to restart the
 * current operation and proceed successfully on the next attempt.
 */
void sched_restart_transaction() NORETURN;

/** @brief Abandon kernel path and find something useful to do.
 *
 * Causes the current kernel path to be abandoned by forcibly
 * clobbering the stack and entering the dispach path. This releases
 * all locks and pins, then calls sched_low_level_yield.
 *
 * The value of @tt current is zeroed. When the low-level yield path
 * is entered a new process will be selected and allowed to attempt
 * a transaction.
 */
void sched_abandon_transaction() NORETURN;

/** @brief Enter the commit point in this operation.
 *
 * May restart the current transaction;  if this function returns, 
 * the process *must* complete the current transaction.
 *
 * At the moment, this does nothing, but it is required for static analysis.
 */
static inline void sched_commit_point() { }

/** @brief Select the next process to run.
 *
 * If the process /current/ is still eligable to run, this should
 * return /current/.
 */
struct Process *sched_choose_next();

/** @brief Dispatch some process on the current CPU.
 *
 * Selects the best process to run using sched_choose_next() and
 * dispatches it using proc_dispatch(). Iterates until something is
 * dispatched successfully.
 */
void sched_dispatch_something() NORETURN;

#endif /* __KERNINC_SCHED_H__ */
