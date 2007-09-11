#ifndef __KERNINC_RCVQUEUE_H__
#define __KERNINC_RCVQUEUE_H__
/*
 * Copyright (C) 2006, The EROS Group, LLC.
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
 * @brief Definition of Wrapper structure. */

#include <kerninc/ccs.h>
#include <kerninc/capability.h>
#include <kerninc/ObjectHeader.h>


/** @brief The RcvQueue structure.
 */
struct RcvQueue {
  ObjectHeader      hdr;

  // THIS IS INCOMPLETE
};
typedef struct RcvQueue RcvQueue;

extern void rcvq_gc(RcvQueue *);

#endif /* __KERNINC_RCVQUEUE_H__ */
