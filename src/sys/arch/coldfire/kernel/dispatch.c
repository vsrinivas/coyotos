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
 * @brief Dealing with process dispatch
 */

#include <kerninc/printf.h>
#include <kerninc/Process.h>
#include <kerninc/Mapping.h>

void proc_arch_initregs(uva_t pc)
{
  Process *p = MY_CPU(current);

  p->state.regs.fix.pc = pc;

  /* On Coldfire the privileged state does not reside in the CCR, so
   * we don't need to do anything special to initialize that.
   */
}

void 
proc_clear_arch_issues(Process *p)
{
  if (atomic_read(&p->issues) & ~(pi_SysCallDone | pi_Preempted))
    bug("Process has arch issues\n");
}

/**
 * BIG potential issue here.
 *
 * We are about to cause a Mapping structure M to be referenced by
 * some actual CPU. It is possible that a second CPU is presently
 * trying to allocate a Mapping structure, and that it will chance to
 * attempt to allocate M. No lock is held on the actual Mapping
 * structure, so there is a race here.
 *
 * Note that in this case the table we are loading will be referenced
 * by the PTE-1 mapping. The acquiring CPU must shoot down all of the
 * PTE-1 entries before claiming M. In doing so, it will generate an
 * IPI to us and it will wait for us to acknowledge the IPI.
 *
 * The way this will get resolved is as follows:
 *
 * Any path proceeding after proc_migrate_to_cpu() will do one of
 * three things:
 *
 * -# Call sched_abandon_transaction()
 * -# Call sched_restart_transaction()
 * -# Attempt to return to user mode
 *
 * In the first two cases the end of the transaction will include a
 * check for a pending IPI. If a top-level invalidate is pending, we
 * will switch to the kernel map, acknowledge the IPI, and proceed
 * (having cleared the entry).
 *
 * In the last case, the protocol is:
 *
 * - locally disable interrupts
 * - check for pending IPI
 *   - If none pending, return to user land
 *   - If IPI response pending, re-enable interrupts
 *     and call sched_restart_transaction().
 *
 * So all cases are resolved.
 */
void
proc_migrate_to_current_cpu()
{
  Process *p = MY_CPU(current);
  if (p->mappingTableHdr == 0)
    p->mappingTableHdr = &KernMapping;
}
