#ifndef __KERNINC_LINK_H__
#define __KERNINC_LINK_H__
/*
 * Copyright (C) 1998, 1999, 2005, Jonathan S. Shapiro.
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
 * @brief Circularly linked list support.
 *
 * Circularly linked list support.  Note that these structures must always
 * be initialized before use, since an empty list points back to itself.
 */

#include <stdbool.h>
#include <kerninc/assert.h>

/** @brief Circularly linked list structure */
struct Link {
  struct Link *next;
  struct Link *prev;
} ;
typedef struct Link Link;

/** @brief Initialize a circularly linked list head */
static inline void
link_init(Link *thisPtr)
{
  thisPtr->prev = thisPtr;
  thisPtr->next = thisPtr;
}

/** @brief Return true iff this list is empty. */
static inline bool
link_isSingleton(Link *l)
{
  return (l->next == l);
}

/** @brief Unlink a circularly linked list element 
 *
 * @bug Does this work if the item is already unlinked? */
static inline void 
link_unlink(Link* thisPtr) 
{
  thisPtr->next->prev = thisPtr->prev;
  thisPtr->prev->next = thisPtr->next;
  thisPtr->next = thisPtr;
  thisPtr->prev = thisPtr;
}

/** @brief Insert a circularly linked list element after an existing
    element. */
static inline void
link_insertAfter(Link *inList, Link *newItem)
{
  assert(link_isSingleton(newItem));
  assert(inList->next);
  assert(inList->prev);

  newItem->next = inList->next;
  newItem->prev = inList;
  inList->next->prev = newItem;
  inList->next = newItem;
}

/** @brief Insert a circularly linked list element before an existing
    element. */
static inline void
link_insertBefore(Link *inList, Link *newItem)
{
  assert(inList->prev);
  link_insertAfter(inList->prev, newItem);
}

#endif /* __KERNINC_LINK_H__ */
