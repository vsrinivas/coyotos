
/*
 * Copyright (C) 2007, The EROS Group, LLC.
 *
 * This file is part of the EROS Operating System.
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
 * @brief Stall and Ready Queue management
 */

#include <kerninc/ReadyQueue.h>

DEFREADYQUEUE(mainRQ);

bool 
sq_IsEmpty(StallQueue* sq)
{
  SpinHoldInfo shi = spinlock_grab(&sq->qLock);
  bool result= link_isSingleton(&sq->q_head);
  spinlock_release(shi);
  return result;
}

static Process *
sq_removeFront(StallQueue *sq)
{
  Process *p = NULL;
  SpinHoldInfo shi = spinlock_grab(&sq->qLock);
  if (!sq_IsEmpty(sq)) {
    Link *ptr = sq->q_head.next;
    link_unlink(ptr);

    p = process_from_link(ptr);
    p->onQ = NULL;
  }
  spinlock_release(shi);
  return p;
}

void 
sq_WakeAll(StallQueue* sq, bool verbose /*@ default false @*/)
{
  ReadyQueue *rq = &mainRQ;

  SpinHoldInfo shi = spinlock_grab(&sq->qLock);
  if (!link_isSingleton(&sq->q_head)) {
    SpinHoldInfo rshi = spinlock_grab(&rq->queue.qLock);
    /* we could move them in a gang, but this is easier to understand */
    while (!link_isSingleton(&sq->q_head)) {
      Link *ptr = sq->q_head.next;
      link_unlink(ptr);

      Process *p = process_from_link(ptr);
      p->onQ = &rq->queue;
      link_insertBefore(&rq->queue.q_head, ptr);
    }
    spinlock_release(rshi);
  }
  spinlock_release(shi);
  return;

}

void
sq_EnqueueOn(StallQueue *sq)
{
  Process *p = MY_CPU(current);

  SpinHoldInfo shi = spinlock_grab(&sq->qLock);
  p->onQ = sq;
  link_insertAfter(&sq->q_head, process_to_link(p));
  spinlock_release(shi);
}

void
sq_Unsleep(Process *process)
{
  assert(process->onQ != 0);

  StallQueue *sq;
  SpinHoldInfo shi;
  for (;;) {
    sq = process->onQ;
    if (sq == 0 || sq == &mainRQ.queue)
      return;
    shi = spinlock_grab(&sq->qLock);
    if (sq == process->onQ)
      break;
    spinlock_release(shi);
  }
  SpinHoldInfo rshi = spinlock_grab(&mainRQ.queue.qLock);

  Link *cur = process_to_link(process);
  link_unlink(cur);
  process->onQ = &mainRQ.queue;
  link_insertBefore(&mainRQ.queue.q_head, cur);

  spinlock_release(rshi);
  spinlock_release(shi);
}

void 
rq_add(ReadyQueue *queue, Process *process, bool at_front)
{
  Link *cur = process_to_link(process);
  SpinHoldInfo shi = spinlock_grab(&queue->queue.qLock);
  process->onQ = &queue->queue;
  if (at_front)
    link_insertAfter(&queue->queue.q_head, cur);
  else
    link_insertBefore(&queue->queue.q_head, cur);
  spinlock_release(shi);
}

/** @brief remove process from queue
 * 
 * @bug this must use more state in the process to make sure it's actually
 * on the ready queue. Did shap fix this by adding the isSingleton
 * test?
 *
 * @bug Is there actually any reason for this to require a ReadyQueue
 * as an argument?
 */
void 
rq_remove(ReadyQueue *queue, Process *process)
{
  assert(process->onQ == &queue->queue);

  SpinHoldInfo shi = spinlock_grab(&queue->queue.qLock);
  Link *cur = process_to_link(process);
  assert(!link_isSingleton(cur));
  process->onQ = NULL;
  link_unlink(cur);
  spinlock_release(shi);
}

Process *rq_removeFront(ReadyQueue *queue)
{
  return (sq_removeFront(&queue->queue));
}
