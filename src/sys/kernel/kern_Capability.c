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
 * @brief Capability management.
 */

#include <kerninc/string.h>
#include <kerninc/assert.h>
#include <kerninc/capability.h>
#include <kerninc/printf.h>
#include <kerninc/Cache.h>
#include <kerninc/Depend.h>
#include <kerninc/GPT.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectHash.h>
#include <kerninc/Process.h>
#include <kerninc/Endpoint.h>
#include <kerninc/ObStore.h>
#include <kerninc/InvParam.h>
#include <coyotos/syscall.h>
#include <hal/atomic.h>
#include <hal/syscall.h>


StallQueue obStallQueue[N_OBSTALLQUEUE];

void
obhdr_stallQueueInit(void)
{
  size_t idx;
  for (idx = 0; idx < N_OBSTALLQUEUE; idx++)
    sq_Init(&obStallQueue[idx]);
}

/** @brief Deprepare a stale capability by either by turning it into a
 * Null capability (if destroyed), or by unswizzling it
 * (otherwise). Returns true if cap remains valid.
 *
 * Precondition: cap is prepared.
 */
bool
cap_rewrite_deprepared(capability *cap)
{
  OTEntry *otEntry = cap->u2.prepObj.otIndex;

  if (atomic_read(&otEntry->flags) & OTE_DESTROYED) {
    cap_init(cap);		/* to Null */
    return false;
  }

  cap->swizzled = 0;
  cap->u2.oid = otEntry->oid;
  return true;
}

void
cap_gc(capability *cap)
{
  if (cap_isStalePrep(cap))
    cap_rewrite_deprepared(cap);

  /* No need to mark OTE, since if the OTE is in use it will be
     pointed to by some object header and we will mark it then. */
}

/** @brief Copy capability from @p src to @p dest.
 *
 * Caller must hold locks on both containing objects.
 */
void
cap_set(capability *dest, capability *src)
{
  // Assist background GC (if any) by doing a cap_gc() on the source
  // capability before copying. This serves to ensure that if the
  // target address falls below the GC mark pass progress threshold
  // the outcome of the copy will not violate the GC invariants.
  cap_gc(src);

  memcpy(dest, src, sizeof(*dest));
}

/**
 * @brief attempt to lock a prepared capability's target ObjectHeader.
 *
 * Fails if the capability is not prepared, or the preparation is out
 * of date.
 */
static ObjectHeader *
cap_preplock(capability *cap, HoldInfo *out)
{
  if (!cap->swizzled)
    return 0;

  ObjectHeader *obj = cap->u2.prepObj.target;

  HoldInfo hi = mutex_grab(&obj->lock);
  OTEntry *capCur = cap->u2.prepObj.otIndex;
  OTEntry *cur = atomic_read_ptr(&obj->otIndex);
  if (capCur != cur) {
    if (capCur != OTINDEX_UNCHECKREF(cur)) {
      mutex_release(hi);
      return 0;
    }
    cache_upgrade_age(obj, capCur);
    assert(atomic_read_ptr(&obj->otIndex) == capCur);
  }
  if (out)
    *out = hi;
  return obj;
}

ObjectHeader *
cap_prepAndLock(capability *cap, HoldInfo *hi)
{
  if (!cap_canPrepare(cap))
    return 0;

  if (cap->swizzled) {
    ObjectHeader *obj = cap_preplock(cap, hi);

    // Capability may have been stale, in which case cap_preplock()
    // returned NULL:
    if (obj) return obj;

    if (!cap_rewrite_deprepared(cap))
      return 0;
  }

  /* Capability was not prepared in any form. Prepare it. */
  ObType ot = captype_to_obtype(cap->type);
  assert(ot != ot_Invalid);

  /// @bug There is a race here between checking the bucket and
  /// initiating the I/O that populates the bucket. Shap
  /// provisionally thinks that the I/O should be initiated with the
  /// hash bucket lock retained, in which case the obhash_XXX()
  /// function preconditions need to be reconsidered.

  /* Following lookup returns locked target object. */
  ObjectHeader *obj = 
    obstore_require_object(ot, cap->u2.oid, true /* waitForRange */, hi);

  if (obj->allocCount != cap->allocCount) {
    cap_init(cap);
    return 0;
  }

  // Set up an OTE, if necessary.
  OTEntry *ote = atomic_read_ptr(&obj->otIndex);
  if (ote == OTINDEX_INVALID) {
    ote = cache_alloc_OTEntry();
    ote->oid = obj->oid;
    cache_upgrade_age(obj, ote);
  } else if (OTINDEX_IS_CHECKREF(ote)) {
    cache_upgrade_age(obj, OTINDEX_UNCHECKREF(ote));
  }

  /* Prepare was successful. Update the capability */
  cap->swizzled = 1;
  cap->u2.prepObj.otIndex = OTINDEX_UNCHECKREF(atomic_read_ptr(&obj->otIndex));
  cap->u2.prepObj.target = obj;
  return obj;
}

Endpoint *
cap_prepare_for_invocation(InvParam_t *iParam, capability *cap, 
			   bool willingToBlock, bool selfOK)
{
  HoldInfo hi;
  ObjectHeader *hdr = cap_prepAndLock(cap, &hi);

  if (hdr == 0)
    return NULL;

  if (cap->type != ct_Entry)
    return NULL;

  Endpoint *ep = (Endpoint *)cap->u2.prepObj.target;
  if (ep->state.pm && (ep->state.protPayload != cap->u1.protPayload)) {
    /* Protected payload has failed to match. Cap is no longer
     * valid. Back out carefully, releasing lock on target object. */
    mutex_release(hi);
    cap_init(cap);
    return NULL;
  }

  capability *pCap = &ep->state.recipient;

  /* Prepare the target process. */
  cap_prepare(pCap);

  /* Enforced by endpoint setTarget() method and by MKIMAGE. */
  assert ((pCap->type == ct_Null) || (pCap->type == ct_Process));
  
  /* Endpoint may contain Null recipient cap if target process was
     destroyed. If we are willing to block, wait for fixup. */
  if (pCap->type == ct_Null) {
    if (willingToBlock)
      obhdr_sleepOn(&ep->hdr);

    return NULL;
  }

  Process *p = (Process *)pCap->u2.prepObj.target;
  /* We have a prepared process;  check to see if it is receiving */
  bool validState = ((selfOK && p == iParam->invoker)
		     || p->state.runState == PRS_RECEIVING);

  if (!validState) {
    if (willingToBlock)
      sq_SleepOn(&p->rcvWaitQ);
    
    return NULL;
  }

  /* If target process is in a closed wait, but not on this endpoint,
   * we may need to block. */

  uintptr_t invokee_icw = get_icw(p);
  if ((invokee_icw & IPW0_CW) && 
      (get_rcv_epID(p) != ep->state.endpointID)) {

    /* Receiver in closed wait on something else. If we are
     * unwilling to block, proceed to receive phase. Policy:
     * reply cap has not been successfully invoked, so do not
     * bump PP. */
    if (willingToBlock)
      sq_SleepOn(&p->rcvWaitQ);

    return NULL;
  }

  iParam->invokee = p;
  iParam->invokeeEP = ep;

  return ep;
}


void
cap_deprepare(capability *cap)
{
  if (!cap->swizzled)
    return;

  HoldInfo hi;
  ObjectHeader *hdr = cap_preplock(cap, &hi);

  if (hdr) {			/* Target was not stale */
    hdr->hasDiskCaps = true;
    mutex_release(hi);
  }

  cap_rewrite_deprepared(cap);
}

void
cap_init(capability *cap)
{
  INIT_TO_ZERO(cap);
  cap->type = ct_Null;
}

/**
 * Note that cap_weaken() is always called in the middle of a
 * capability copy.
 */
void
cap_weaken(capability *cap)
{
  switch(cap->type) {
  case ct_Null:
  case ct_Window:
  case ct_LocalWindow:
  case ct_Discrim:
    return;
  case ct_GPT:
  case ct_CapPage:
    cap->restr = CAP_RESTR_RO | CAP_RESTR_WK;
    return;

  case ct_Page:
    cap->restr = CAP_RESTR_RO;
    return;

  default:
    /// @bug One can imagine a RO+WK version of a Process
    /// capability. Should we introduce that?
    cap_init(cap);
  return;
  }
}

void
obhdr_dirty(ObjectHeader *hdr)
{
  assert(mutex_isheld(&hdr->lock));

  assert(!hdr->snapshot);
  assert(hdr->current);

  hdr->dirty = 1;
}

static void
process_invalidate(Process *p)
{
  /** @bug  XXX need invalidation logic */
  bug("Implement process_invalidate\n");
}

void
memhdr_invalidate_cached_state(MemHeader *hdr)
{
  if (hdr->hdr.ty == ot_GPT)
    depend_invalidate((GPT *)hdr);

  memhdr_destroy_products(hdr);
}

void
obhdr_invalidate(ObjectHeader *hdr)
{
  OTEntry *ote = atomic_read_ptr(&hdr->otIndex);
  if (ote == OTINDEX_INVALID)
    return;

  switch (hdr->ty) {
  case ot_Page:
  case ot_CapPage:
  case ot_GPT:
    memhdr_invalidate_cached_state((MemHeader *)hdr);
    break;
  case ot_Process:
    process_invalidate((Process *)hdr);
    break;
  case ot_Endpoint:
    /* nothing to do */
    break;
  default:
    bug("No invalidation logic for header type %d", hdr->ty);
    break;
  }

  /* now that all of the cached state is gone, invalidate the otIndex */

  /* we need to mark it, since GC may still be in the OTE mark pass */
  uint32_t flags;
  do {
    flags = atomic_read(&ote->flags);
    if (flags & OTE_MARK)
      break;
  } while (compare_and_swap(&ote->flags, flags, flags | OTE_MARK) != flags);

  atomic_write_ptr(&hdr->otIndex, OTINDEX_INVALID);
}
