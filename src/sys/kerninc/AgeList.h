#ifndef __KERNINC_AGELIST_H__
#define __KERNINC_AGELIST_H__
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
 *
 * @brief Type definition and functions for Aging lists.
 */

#include <kerninc/Link.h>
#include <kerninc/assert.h>
#include <kerninc/string.h>

/**
 * @brief AgeList structure.
 *
 * Age lists abstract out lists of ageable objects.  Objects can be removed
 * from any point in the list, but are only added to the front and back of
 * the list.  The list maintains a count of items on it.
 *
 * The Agelist structure does no locking; the caller must provide
 * mutual exclusion.
 *
 * All items on a given AgeList should be of the same type, and *must* have
 * a "Link" as their first element, which is used for the aging list.
 */
typedef struct AgeList {
  /** @brief Root of age list */
  Link		list;
  /** @brief Number of items on Age list */
  size_t	count;
  /** @brief count fell below LowWater, but has not reached HighWater yet. */
  bool		needsFill;
  /** @brief Low watermark; if its count goes below this, the list should be
   * filled to hiWater
   */
  size_t	lowWater;
  /** @brief High water mark. */
  size_t	hiWater;
} AgeList;

/** @brief Initialize an agelist structure */
static inline void 
agelist_init(AgeList *al)
{
  INIT_TO_ZERO(al);
  link_init(&al->list);
}

/** @brief Returns true if count has dropped below the low watermark */
static inline bool 
agelist_underLowWater(AgeList *al)
{
  return (al->count < al->lowWater);
}
 
/** @brief Returns true if count has dropped below the high watermark */
static inline bool 
agelist_underHiWater(AgeList *al)
{
  return (al->count < al->hiWater);
}

/** @brief Returns true if count is zero */
static inline bool 
agelist_isEmpty(AgeList *al)
{
  return (al->count == 0);
}

/** @brief Returns true if count is greater than amount */
static inline bool 
agelist_hasMoreThan(AgeList *al, size_t count)
{
  return (al->count > count);
}
 
/** @brief Find the oldest item on the list, and return it. */
static inline void *
agelist_oldest(AgeList *al)
{
  if (link_isSingleton(&al->list))
    return 0;
  return (al->list.prev);
}

/** @brief Remove an item from the agelist; it is the caller's
 * responsibility to guarantee that the object is on the passed in
 * list.
 */
static inline void 
agelist_remove(AgeList *al, void *oldest)
{
  link_unlink((Link *)oldest);
  assert(al->count > 0);
  al->count--;
}

/** @brief Remove the requested item, which must be the oldest item on
 * the list.
 */
static inline void 
agelist_removeOldest(AgeList *al, void *oldest)
{
  assert(agelist_oldest(al) == oldest);
  agelist_remove(al, oldest);
}

/** @brief Add an item to the front of the list, making it the "newest". */
static inline void
agelist_addFront(AgeList *al, void *newFront)
{
  link_insertAfter(&al->list, newFront);
  al->count++;
}

/** @brief Add an item to the back of the list, making it the "oldest". */
static inline void
agelist_addBack(AgeList *al, void *newTail)
{
  link_insertBefore(&al->list, newTail);
  al->count++;
}

#endif /* __KERNINC_AGELIST_H__ */
