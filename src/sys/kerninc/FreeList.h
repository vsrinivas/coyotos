#ifndef __KERNINC_FREELIST_H__
#define __KERNINC_FREELIST_H__
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

/** @file 
 * @brief Low-level free lists.
 *
 * Free lists are manipulated by non-blocking data structures, because
 * we need to be able to release things after the commit point, and
 * that implies that we must not spin.
 */

#include <stdbool.h>
#include <hal/atomic.h>

typedef struct FreeListElem FreeListElem;
struct FreeListElem {
  FreeListElem *next;
};

typedef struct FreeListHeader FreeListHeader;
struct FreeListHeader {
  AtomicPtr_t next;		/* FreeListElem* */
};

static inline FreeListElem *
freelist_alloc(FreeListHeader *flh)
{
  FreeListElem *next = atomic_read_ptr(&flh->next);

  for(;;) {
    FreeListElem *was = CAS_PTR(FreeListElem *, &flh->next, next, next->next);
    if (was == next) {
      if (next) next->next = 0;
      return next;
    }
    next = was;
  }
}

static inline void
freelist_insert(FreeListHeader *flh, void *vp)
{
  FreeListElem *ob = vp;
  FreeListElem *first = atomic_read_ptr(&flh->next);

  for (;;) {

    ob->next = first;

    FreeListElem *was = CAS_PTR(FreeListElem *, &flh->next, first, ob);
    if (was == first)
      return;

    first = was;
  }
}

static inline void
freelist_init(FreeListHeader *flh)
{
  flh->next.vp = 0;
}

#endif /* __KERNINC_FREELIST_H__ */
