#ifndef __KERNINC_IPI_H__
#define __KERNINC_IPI_H__
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
 * @brief Inter-processor interrupt messages
 */

/* Invalidate TLB */
/* Invalidate VA in TLB */
/* Relinquish running process */
/* Snapshot (No-OP, used to force CPUs to take notice of
   snapshot events */

/* PROTOCOL:
 *
 * A sets up some arguments somewhere.
 *
 * A ships B (and possibly to others) an IPI(fromA). A either keeps
 * bitmap of unprocessed IPI recipients or a count of them.
 *
 * A spins for IPI completion from all CPUs whose heads are currently
 * bashed in.
 *
 * B marks a per-CPU bit meaning "IPI interrupt received from A"
 * (rule: L2 interrupts do not logically preempt main line
 * processing). Acknowledges the HW-level interrupt eagerly. This
 * means "message received", not to be confused with "message
 * processed".
 *
 * At some convenient point, B "notices" pending IPI from A.
 *
 *   B examines A's stashed arguments.
 *
 *   B does requested op.
 *
 *   B acknowledges completion of IPI processing to A, either by
 *     atomic count decrement or CASing A's  bitmask.
 *
 * Eventually all victims have acknowledged and A can proceed.
 *
 * REQUIREMENTS:
 *   CPU X may only send IPIs when it is permissable for X to Yield.
 *
 *   No IPI processing action may send an IPI.
 *
 *   No IPI processing action may Yield.
 *
 *   No IPI processing should take any lock. It is okay to @em release
 *   locks. Nova also okay. Capers, tomatos, and onions are good with
 *   that.
 *
 *   CPU X may not send an IPI while holding the ready queue spinlock.
 *
 *   CPU X may not send IPI after commit point.
 *
 *   CPU X may not process an incoming IPI between commit point and
 *     operation completion.
 *
 * NOTES:
 *
 *  While A is waiting for IPI receiver completions, A may be the
 *  recipient of one (or more) IPIs. Note that if A is issuing an IPI,
 *  A is (by construction) in a position to abort or restart its
 *  current transaction.
 *
 *  If an incoming IPI happens during the IPI completion wait loop, A
 *  takes interrupt, marks self as IPI recipient, returns to IPI
 *  completion wait spin loop, notices IPI receive. Immediately
 *  processes received IPI without exiting spin loop, but also takes
 *  note that A must yield after all expected IPI receives have
 *  completed (cannot abort in mid-loop).
 *
 *  Implication: it actually *is* okay for A to send an IPI between
 *  the time A gets an incoming IPI hardware interrupt and the time A
 *  processes the IPI request and responds.
 *
 *  In the case of the "relinquish running process" IPI, completion
 *  implies that the requestor now has exclusive dominion over the
 *  requested process structure. It is incumbent upon the requestor to
 *  stick that process back at the @b front of the ready queue before
 *  yielding. In particular, this may require recognition within the
 *  IPI completion wait loop if we are yielding from within the
 *  completion loop.
 */
#endif /* __KERNINC_IPI_H__ */
