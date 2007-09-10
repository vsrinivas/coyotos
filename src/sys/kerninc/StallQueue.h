#ifndef __KERNINC_STALLQUEUE_H__
#define __KERNINC_STALLQUEUE_H__
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
 *
 * @brief Type definition and functions for stall queues.
 */

#include <kerninc/Link.h>
#include <kerninc/mutex.h>
#include <kerninc/Sched.h>

/**
 * @brief Stall queue structure.
 *
 * Stall Queues are simply link structures with a lock.
 */
struct StallQueue {
  spinlock_t qLock;

  /* @bug NEED LOCK HERE */
  Link q_head;
};
typedef struct StallQueue StallQueue;

/** @brief Forcibly re-initialize a stall queue. */
static inline void
sq_Init(StallQueue *sq)
{
  link_init(&sq->q_head);
}

/** @brief Return true iff the stall queue has no members. */
bool sq_IsEmpty(StallQueue* sq);

/** @brief Wake all processes on this stall queue. */
void sq_WakeAll(StallQueue* sq, bool verbose /*@ default false @*/);

void sq_EnqueueOn(StallQueue *sq);

/** @brief remove a Process from the queue it is on, and put it on the
 * runqueue.
 *
 * If the process is not sleeping on a queue, or is on a RunQueue already,
 * this has no effect.
 */
void sq_Unsleep(struct Process *process);

static inline void sq_SleepOn(StallQueue *sq) {
  sq_EnqueueOn(sq);
  sched_abandon_transaction();
}


#define STALLQUEUE_INIT(name) { SPINLOCK_INIT, { &name.q_head, &name.q_head } }
#define DEFQUEUE(name) \
  struct StallQueue name = STALLQUEUE_INIT(name);

#endif /* __KERNINC_STALLQUEUE_H__ */
