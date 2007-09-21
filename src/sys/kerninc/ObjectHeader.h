#ifndef __KERNINC_OBJECTHEADER_H__
#define __KERNINC_OBJECTHEADER_H__
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
 * @brief Definition of common object header structure. */

#include <stdbool.h>
#include <kerninc/mutex.h>
#include <kerninc/Link.h>
#include <kerninc/StallQueue.h>
#include <hal/kerntypes.h>

/** @brief Types of object cache entries. */
typedef enum ObType {
#define DEFFRAME(ft, val) ot_ ## ft = val,
#define ALIASFRAME(ft, alias, val) ot_ ## ft = val,
#include <kerninc/frametype.def>
} ObType;

enum { 
#define NODEFFRAME(ft, val) ot_ ## ft = val,
#include <kerninc/frametype.def>
  ot_Invalid = ot_NTYPE 
};

/** @brief The object header structure.
 *
 * This is a common header structure for every entry in every kernel
 * object cache. Add fields with hesitation, because a single word
 * added here gets multiplied many times over.
 *
 * @bug This may be misnamed, since it is really the *frame* header.
 *
 * @section Locking
 *
 * Most fields of the ObjectHeader are protected by the @p lock field.  The
 * exceptions are:
 * @li
 *    @p ageLink, which is protected by the ObFrameCache lock field.
 * @li
 *    @p next, which is protected by the obhash mutex for (@p ty, @p oid)
 * @li
 *    @p ty and @p oid, which are protected by @p lock, but cannot change
 *    as long as the object is on an obhash chain.
 * @li
 *    @p otIndex, which is read without holding a lock.  It cannot be
 * changed without holding the ObFrameCache lock, since it is part of the
 * aging state.
 *
 * Most fields in structures which contain an ObjectHeader are also
 * protected by the ObjectHeader's @p lock field.
 */
typedef struct ObjectHeader {
  /** @brief Links in object history list
   *
   * MUST BE FIRST!! 
   */
  Link     ageLink;

  /** This field has different uses according to the allocation state
   * of this object. If this object is allocated, then this is
   * the object hash table next pointer.
   */
  struct ObjectHeader *next;

  uint64_t oid;
  uint32_t allocCount : 20;

  mutex_t  lock;   /* protects header and object state */

  /** @brief Object type */
  ObType ty;

  /** @brief Current OT index.
   *
   * Really an (OTEntry *) 
   *
   * If the low bit is set, this capability is on the "check" aging list,
   * and should be upgraded on any prepare.
   */
  AtomicPtr_t otIndex;

  /** @brief Set to true whenever a valid capability to this object is
   * deprepared.
   *
   * This occurs when the object containing that capability is on its
   * way to the store.
   */
  bool hasDiskCaps;

  /** @brief Object is the most current version of the object */
  bool current;

  /** @brief Object is involved in a snapshot */
  bool snapshot;

  /** @brief Object has been modified and requires write-back. */
  bool dirty;

  /** @brief Object is pinned and cannot be aged out */
  bool pinned;

  /** @brief Object checksum.
   *
   * Should be valid if object is !modified, or if object is modified
   * and not a data page. N.B. that checksum is computed based on the
   * deprepared form of capabilities. That is: **as if** the slots had
   * been deprepared. This is consistent with the concept that
   * preparing a capability is not a logical mutate.
   */
  uint32_t cksum;
} ObjectHeader;

/**
 * @brief Object Header varient for object which can be in memory hierarchies.
 *
 * @section Locking.
 * 
 * The @p products field is protected by a global products lock.
 */
typedef struct MemHeader {
  ObjectHeader hdr;

  struct Mapping *products;
} MemHeader ;

typedef struct Page {
  MemHeader mhdr;

  /** @brief Physical address of page */
  kpa_t  pa;
} Page;

extern void page_gc(Page *page);

/**
 * @brief Mark an object header dirty in preperation for modification.
 *
 * Note that this is only needed for modification to externally visible state.
 *
 * Preconditions:  
 *     Must be called before the commit point,
 *     The ObjectHeader's lock must be held.
 *
 * Postconditions:
 *     The object is marked dirty and is safe to be written to after 
 *     the commit point.
 */
extern void obhdr_dirty(ObjectHeader *);

/** @brief Remove all content of an object header, invalidating all
 * cached state including outstanding capabilities.
 *
 * @bug This is misnamed. What it @em actually does is invalidate all
 * of the dependent cache state.
 */
extern void obhdr_invalidate(ObjectHeader *);

/** @brief Invalidate all cached state generated by this memheader. */
extern void memhdr_invalidate_cached_state(MemHeader *);

/** @brief Make all products of this MemHeader unreachable. */
__hal extern void memhdr_invalidate_products(MemHeader *);

/** @brief Invalidate and destroy all products of this MemHeader. */
__hal extern void memhdr_destroy_products(MemHeader *);

#define N_OBSTALLQUEUE 128
extern StallQueue obStallQueue[N_OBSTALLQUEUE];

/** @brief Return the stall queue associated with @p ob.
 *
 * This hash is really sophisticated... :-) */
static inline StallQueue *obhdr_getStallQueue(ObjectHeader* ob)
{
  return &obStallQueue[ob->oid % N_OBSTALLQUEUE];
}

/** @brief Block until some condition associated with @p ob is likely
 * to have become true.
 *
 * What the "condition" is is varied. Wakeup may happen early, since
 * the queue may be shared across multiple objects. */
static inline void obhdr_sleepOn(ObjectHeader* ob)
{
  sq_SleepOn(obhdr_getStallQueue(ob));
}

/** @brief Wake all of the lazy duffers who are waiting for something
 * to happen on @p ob.
 */
static inline void obhdr_wakeAll(ObjectHeader* ob)
{
  sq_WakeAll(obhdr_getStallQueue(ob), false);
}

/** @brief Initialize the stall queue array. */
extern void obhdr_stallQueueInit(void);

/** @brief Find a Page header by physical address. */
extern Page *obhdr_findPage(kpa_t pa);

#endif /* __KERNINC_OBJECTHEADER_H__ */
