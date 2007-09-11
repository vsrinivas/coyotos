#ifndef __KERNINC_CAPABILITY_H__
#define __KERNINC_CAPABILITY_H__
/*
 * Copyright (C) 2006, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Kernel-internal capability interfaces.
 */

#include <obstore/capability.h>
#include <kerninc/mutex.h>
#include <kerninc/ObjectHeader.h>
#include <hal/atomic.h>

/** @brief Per-object OT (Object Table) Entry.
 *
 * Per-object OT (Object Table) Entry.
 *
 * Preparing a capability involves:
 * 
 * @li A look-up in the ObjectHash to find the ObjectHeader corresponding
 *     to the capability's type and OID.  If it isn't found, the object
 *     must be brought in from disk, and we YIELD().
 * @li The ObjectHeader is locked, and the allocation count in the object
 *     is compared to the allocation count in the capability.  If it doesn't
 *     match, the prepare fails, and the capability is nullified.
 * @li If the object's otIndex is OTINDEX_INVALID, a new ObjectTable entry
 *     is allocated (causing incremental GC), the OID of the object is written
 *     into the OT entry, and the index of that entry is written into the
 *     ObjectHeader's otIndex field.
 * @li The capability is marked "prepared", and the ObjectHeader and its 
 *     otIndex are written into the capability, overwriting its OID field.
 * @li The ObjectHeader's lock is dropped.
 *
 * The capability's preperation is valid as long as its otIndex
 * matches the ObjectHeader's otIndex.  When the object is destroyed
 * or removed from memory, its current OTEntry's "destroyed" field is
 * set if the object was destroyed, and the object's otIndex is overwritten
 * with OTINDEX_INVALID.
 *
 * Once a capability's preperation is invalidated 
 *    (that is, cap->otIndex != cap->target->otIndex)
 * the capability can be deprepared.  If "destroyed" is set in the
 * OTEntry, the capability is nullified.  If not, the object still exists,
 * so the OID from the OTEntry overwrites the cap's otIndex and target fields,
 * and cap->swizzled is cleared.
 * 
 */

#define OTE_DESTROYED  0x1u
#define OTE_MARK       0x2u
#define OTE_FREE       0x4u
typedef struct OTEntry {
  uint64_t oid;

  /* Very important that these bits do not appear in the first word,
     because they can be manipulated during GC even if the OTE is on
     the free list. */

  /** @brief object was destroyed: */
  Atomic32_t flags;
} OTEntry;

#define OTINDEX_INVALID 0

#define OTINDEX_CHECKREF(x) (OTEntry *)((uintptr_t)(x) | (uintptr_t)1)
#define OTINDEX_UNCHECKREF(x) (OTEntry *)((uintptr_t)(x) & ~(uintptr_t)1)

#define OTINDEX_IS_CHECKREF(x) (bool)((uintptr_t)(x) & (uintptr_t)1)

/**
 * @brief Convert a capability type to an Object type.  Returns ot_Invalid
 * for capability types which have no corresponding Object types.
 */
static inline ObType
captype_to_obtype(CapType t)
{
  switch (t) {
#define DEFCAP(ty, ft, val) case val: return ot_##ft;
#define NODEFCAP(x, y, z)
#include <obstore/captype.def>
#undef DEFCAP
#undef NODEFCAP
  default:  return ot_Invalid;
  }
}

/** @brief check that a guard is valid */
static inline bool guard_valid(guard_t g)
{
  uint32_t l2g = guard_l2g(g);
  uint32_t match = guard_match(g);

  // basic range checks
  if (l2g > COYOTOS_SOFTADDR_BITS || 
      l2g < COYOTOS_PAGE_ADDR_BITS ||
      match >= CAP_MATCH_LIMIT)
    return false;

  // verify that the set match bits don't overflow a coyaddr_t
  coyaddr_t the_guard = ((coyaddr_t)(match << 1) << (l2g - 1));
  return (match == ((the_guard >> 1) >> (l2g - 1)));
}

/** @brief Return true if this capability type is a candidate for
    swizzling. */
static inline bool cap_canPrepare(capability *cap)
{
  return cap->type >= ct_Endpoint;
}

/** @brief Return true if this capability type is NOT candidate for
    swizzling. */
static inline bool cap_notPreparable(capability *cap)
{
  return !cap_canPrepare(cap);
}

/** @brief Return true if capability is prepared.
 *
 * A capability is not considered prepared if its target object has
 * moved, because the pointer to the target object is not valid in
 * this case.
 */
static inline bool cap_isPrepared(capability *cap)
{
  return (cap_notPreparable(cap) ||
	  (cap->swizzled &&
	   (atomic_read_ptr(&cap->u2.prepObj.target->otIndex) ==
	    cap->u2.prepObj.otIndex)));
}

/** @brief Return true if this is prepared and has become stale, and
 * should therefore be deprepared. */
static inline bool cap_isStalePrep(capability *cap)
{
  return (cap->swizzled &&
	  (atomic_read_ptr(&cap->u2.prepObj.target->otIndex) != 
	   cap->u2.prepObj.otIndex));
}

/** @brief Deprepare if target object is destroyed or paged out. */
extern void cap_gc(capability *cap);

/** @brief Initialize a capability to be a Null cap */
extern void cap_init(capability *cap);

/** @brief Overwrite destination cap with source cap. */
extern void cap_set(capability *dest, capability *src);

/** @brief Weaken a capability according to the rules of weak fetch. */
extern void cap_weaken(capability *cap);

/** 
 * @brief Prepare a capability @p cap and lock target object.
 *
 * If capability @p cap is an object capability, locks target
 * object. Returns target object header pointer and lock info (via @p
 * hi).
 *
 * If capability is stale or a non-object capability, returns NULL.
 * Lock info structure is unmodified.
 */
extern struct ObjectHeader *cap_prepAndLock(capability *cap, HoldInfo *hi);

/**
 * @brief Prepare a capability.
 *
 * At completion, the target object is locked in memory for the
 * duration of the current operation.  Using this interface, there is
 * no way to unlock the object, besides the gang-release at the end of
 * the operation.
 */
static inline void cap_prepare(capability *cap)
{
  (void) cap_prepAndLock(cap, 0);
}

/** @brief Deprepare this capability. 
 *
 * Does NOT update the diskCaps field of the target. Returns TRUE if
 * cap was valid at time of deprepare.
 */
extern bool cap_rewrite_deprepared(capability *cap);

/** @brief Deprepare this capability prior to object pageout. */
extern void cap_deprepare(capability *cap);

/**
 * @brief Prepare a capability for invocation
 *
 * Prepares the provided capability. If it is an entry capability,
 * checks the requirements on the Entry object prepares the
 * contained process capability, and checks if the target process is
 * currently receiving.
 *
 * Outcomes:
 *
 * Returns Endpoint pointer if everything goes right: Endpoint
 * contains valid process capability and target process is in
 * receiving state. Returns NULL otherwise.
 *
 * See large block comment in the implementation for an explanation of
 * why this is NOT the same as cap_prepare().
 */
struct InvParam;
extern struct Endpoint *
cap_prepare_for_invocation(struct InvParam *iParam, capability *cap, 
			   bool willingToBlock, bool selfOK);

#endif /* __KERNINC_CAPABILITY_H__ */
