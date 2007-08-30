#ifndef __KERNINC_READYQUEUE_H__
#define __KERNINC_READYQUEUE_H__
/*
 * Copyright (C) 2003, 2005, The EROS Group, LLC
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
#include <kerninc/Process.h>
#include <kerninc/StallQueue.h>

/**
 * @brief Ready queue structure.
 *
 * Ready Queues are simply Stall Queues with additional state.
 */
struct ReadyQueue {
  StallQueue queue;
};
typedef struct ReadyQueue ReadyQueue;

extern ReadyQueue mainRQ;

extern void rq_add(ReadyQueue *queue, Process *process, bool at_front);
extern void rq_remove(ReadyQueue *queue, Process *process);
extern Process *rq_removeFront(ReadyQueue *queue);

#define DEFREADYQUEUE(name) \
  struct ReadyQueue name = { STALLQUEUE_INIT(name.queue) }

#endif /* __KERNINC_STALLQUEUE_H__ */
