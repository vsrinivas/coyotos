/*
 * Copyright (C) 2006, Jonathan S. Shapiro.
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
 * @brief Support for address space (GPT) manipulation.
 */

#include <string>
#include <libsherpa/UExcept.hxx>
#include <libsherpa/xassert.hxx>

#include "CoyImage.hxx"

using namespace sherpa;

#define GUARD_EXPRESSABLE_BOUND (1ull << CAP_MATCH_WIDTH)

static inline coyaddr_t 
max(coyaddr_t x, coyaddr_t y)
{
  return (x < y) ? y : x;
}

static inline coyaddr_t 
min(coyaddr_t x, coyaddr_t y)
{
  return (x > y) ? y : x;
}

static inline coyaddr_t 
align_up(coyaddr_t x, coyaddr_t y)
{
  return (x + y - 1) & ~(y-1);
}

#define bitsizeof(x) (sizeof(x) * 8)

/** @brief Return the minimum address space size (in bits) sufficient
 * to hold an offset of the magnitude described by @p off.
 */
size_t
CoyImage::l2offset(coyaddr_t off)
{
  size_t sz = 0;
  while (sz < bitsizeof(off) && ((coyaddr_t)1 << sz) <= off)
    sz++;

  if (sz < target.pagel2v)
    sz = target.pagel2v;

  return sz;
}

/// standard integer division:  round up @p a to a multiple of @p b
static inline size_t
round_up(size_t a, size_t b)
{
  return ((a + b - 1)/b) * b;
}

/// clear all bits in @p off from @p bit to 64.
static inline coyaddr_t
lowbits(coyaddr_t off, size_t bit)
{
  if (bit == bitsizeof(coyaddr_t))
    return off;
  return off & ((1ull << bit) - 1ull);
}

/// clear all bits in @p off from @p bit - 1 to 0.
static inline coyaddr_t
hibits(coyaddr_t off, size_t bit)
{
  if (bit == bitsizeof(coyaddr_t))
    return 0;
  return off & ~((1ull << bit) - 1ull);
}

/// Return the bits of @p off from @p bit to 64, shifted to bit position 0
static inline coyaddr_t
hibits_shifted(coyaddr_t off, size_t bit)
{
  if (bit == bitsizeof(coyaddr_t))
    return 0;
  return off >> bit;
}

/// Shift @p off to start at bit position @p bit
static inline coyaddr_t
shift_to_hibits(coyaddr_t off, size_t bit)
{
  if (bit == bitsizeof(coyaddr_t))
    return 0;
  return off << bit;
}

/** @brief Given a capability, return the address space size of the
 * space that it names.
 *
 * This is trickier than it was in EROS because of the guard. In the
 * absence of a guard, a space can be grown up and down very
 * simply. In presence of a guard, splitting of the cap to rewrite the
 * guard may be required.
 *
 * @bug not used
 */
size_t
CoyImage::l2cap(const capability& cap)
{
  if (cap_isMemory(cap))
    return max(cap.u1.mem.l2g, l2offset(cap_guard(cap)));
  else if (cap.type == ct_Null)
    return target.pagel2v;

  return 0;
}

/** @brief Return true if the guard matches up to the given bit. */
static inline bool
match_guard(coyaddr_t guard, coyaddr_t offset, size_t bit)
{
  return (hibits(guard, bit) == hibits(offset, bit)) ? true : false;
}

/** @brief Takes a capability @p spc with no guard, and returns a new
 * capability guarded to be at offset @p offset, with an available size of
 * @p l2arg. 
 *
 * Essentially, we need to apply the @p offset to the memory
 * capability.  At each step, the low bits of the offset are moved
 * into the guard of the capability, if possible.  Then, if there are
 * any non-zero bits left in the offset, a new GPT is created, and the
 * memory capability is put in to its proper slot.  The new GPT
 * becomes the memory capability of interest, and we iterate until
 * offset becomes zero.
 *
 * @p offset must be a multiple of 2^@p l2arg
 */
capability
CoyImage::MakeGuardedSubSpace(capability bank, capability spc,
			      coyaddr_t offset, size_t l2arg)
{
  if (spc.type == ct_Null)
    return spc;		// no need to guard a Null cap; it's invalid-everywhere

  if (!cap_isMemory(spc))
    THROW(excpt::BadValue, "space has incorrect type");

  if (l2arg != 0 && l2arg < target.pagel2v)
    THROW(excpt::BadValue, "l2arg is too small");

  // this entire function relies on the fact that windows and memory
  // caps have the same layout for match and l2g.
  spc.u1.mem.match = 0;

  size_t l2size = (l2arg != 0) ? l2arg : spc.u1.mem.l2g;

  if (lowbits(offset, l2size) != 0)
    THROW(excpt::BadValue, "offset not l2arg-aligned");
  
  // if the passed in l2g is too large, shrink it 
  if (spc.u1.mem.l2g > l2size)
    spc.u1.mem.l2g = l2size;

  while (offset) {
    // Unfortunately, the guard may not fit all of the bits we have
    // left in offset.  We store the offset into the match,
    // truncating any high bits left over, then clear the stored
    // bits from the offset.  If offset ends up non-zero, then we
    // need to add a wrapping GPT, and store spc in the appropriate
    // slot.
    size_t l2g = spc.u1.mem.l2g;
    xassert(lowbits(offset, spc.u1.mem.l2g) == 0);

    // This is safe, since if l2g is bitsizeof(coyaddr_t), then
    // offset must be zero, and we would never enter this loop
    spc.u1.mem.match = (offset >> l2g);
    
    // strip off the bits we actually stored in the guard
    coyaddr_t guardOffset = (coyaddr_t)spc.u1.mem.match << l2g;
    offset &= ~guardOffset;
    
    /* If the guard did not overflow, we are done: */
    if (offset == 0)
      break;
    
    /* Guard overflowed. Need to create a larger space: */
    capability newSpace = AllocGPT(bank);
    GCPtr<CiGPT> gpt = GetGPT(newSpace);

    // apply child's perms to newSpace
    newSpace.restr = (spc.restr & ~CAP_RESTR_OP);
    // end perms

    // cannot be bitsizeof(coyaddr_t), since then offset would be 0.
    gpt->v.l2v = l2g + CAP_MATCH_WIDTH;
    size_t newl2g = min(gpt->v.l2v + GPT_SLOT_INDEX_BITS,
			bitsizeof(coyaddr_t));
    newSpace.u1.mem.l2g = newl2g;
    
    size_t slotBits = newl2g - gpt->v.l2v;
    unsigned ndx = lowbits(offset >> gpt->v.l2v, slotBits);
    
    gpt->v.cap[ndx] = spc;
    
    offset = hibits(offset, newl2g);
    spc = newSpace;
  }
  
  return spc;
}

/**
 * @brief Set up @p spaceGPT so that an insert at @p offset of size @p l2arg
 * can proceed.
 *
 * Modify @p spaceGPT so that an insert at @p offset of size @p l2arg can
 * proceed.  Any storage needed is allocated from @p bank.
 *
 * Normally, traversing a GPT is no problem;  you just pick the appropriate
 * slot and recurse into it.  There are two cases where this won't work:
 *
 *   1. The l2g on the capability pointing to the GPT is larger than
 *      l2v + l2slots, and the offset bits between the two are not all
 *      zero.  Essentially, there aren't enough slots for there to be one
 *      at the requested offset.
 *
 *      The fix is to raise l2v so that the non-zero offset bits fit into
 *      l2v + l2slot.
 *
 *   2. The requested @p l2arg is greater then l2v; that is, the slots
 *      of this GPT are larger than our requester wants.  The fix is to
 *      increase l2v to @p l2arg.
 *
 * In either case, we're increasing l2v;  that requires allocating GPTs
 * for any non-Null slots we have, and filling them with our data.
 *
 * This transform is non-destructive;  the address space before and after
 * is identical.
 */
void
CoyImage::split_GPT(capability bank, GCPtr<CiGPT> spaceGPT,
		    coyaddr_t offset, size_t l2arg)
{
  size_t l2slots = spaceGPT->l2slots();

  size_t l2v = spaceGPT->v.l2v;
  size_t l2vpslot = l2v + l2slots;

  // check if the traverse will just work.  If so, just return.
  if (l2v >= l2arg && hibits(offset, l2vpslot) == 0)
    return;

  // the new l2v we need
  size_t new_l2v;

  if (hibits(offset, l2vpslot) != 0) {
    // the offset doesn't fit into this GPT; we must raise l2v to
    // allow for the offset.

    size_t l2vpslot_desired = l2v + round_up(l2offset(offset) - l2v,
					     GPT_SLOT_INDEX_BITS);

    size_t new_l2vpslot = min(l2vpslot_desired, bitsizeof(coyaddr_t));
    new_l2v = max(new_l2vpslot - l2slots, l2arg);

    xassert(hibits(offset, new_l2vpslot) == 0);
    xassert(new_l2v < new_l2vpslot);
  } else {
    /* l2v < l2arg;  we need to increase l2v */
    new_l2v = l2arg;
  }
  
  xassert(new_l2v > l2v);

  size_t slotMax = 1u << l2slots;

  // raise l2v to new_l2v

  // This loop builds a vector of slotMax capabilities, one for each
  // slot of the final GPT.  Each loop should do exactly one
  // caps.push_back().

  std::vector<capability> caps;

  for (size_t j = 0; j < slotMax; j++) {
    coyaddr_t slotOffset = shift_to_hibits(j, new_l2v);
    if (hibits_shifted(slotOffset, new_l2v) != j) {
      // we're out of the addressable space
      caps.push_back(CiCap());
      continue;
    }
    if (hibits(slotOffset, l2vpslot) != 0) {
      // we're out of the original slot address range
      caps.push_back(CiCap());
      continue;
    }

    size_t lastslot = 0;
    size_t count = 0;
    size_t maxoff = min(slotMax,
			hibits_shifted(shift_to_hibits(1, new_l2v), l2v));
    size_t baseoff = hibits_shifted(slotOffset, l2v);
    xassert(baseoff < slotMax);

    // count the non-Null caps in this range.
    for (size_t off = 0; off < maxoff; off++) {
      if (spaceGPT->v.cap[baseoff + off].type != ct_Null) {
	count++;
	lastslot = off;
      }
    }

    // if all slots are Null, we just use a Null cap.
    if (count == 0) {
      caps.push_back(CiCap());
      continue;
    }

    if (count == 1) {
      // in this case, we don't need to waste a GPT;  we can just put
      // the new_l2v-l2v bits into the existing guard.  We do have to be
      // careful, though;  if the existing cap is *not* a memory cap, or
      // its guard has bits *above* l2v, than the capability is *not*
      // reachable through the slot, and we shouldn't mess with it.
      capability cap = spaceGPT->v.cap[baseoff + lastslot];
      if (cap_isMemory(cap) && hibits(cap_guard(cap), l2v) == 0) {
	coyaddr_t newGuard = cap_guard(cap) | shift_to_hibits(lastslot, l2v);
	caps.push_back(MakeGuardedSubSpace(bank, cap, newGuard,
					   min(l2v, cap.u1.mem.l2g)));
	continue;
      }
    }
    
    // We need to allocate a GPT to hold the capabilities in this slot.
    capability newGPTCap = AllocGPT(bank);
    GCPtr<CiGPT> newGPT = GetGPT(newGPTCap);
    
    newGPT->v.l2v = l2v;
    newGPTCap.u1.mem.l2g =
      min(l2v + GPT_SLOT_INDEX_BITS, bitsizeof(coyaddr_t));
    
    for (size_t off = 0; off < maxoff; off++) {
      capability &cur = spaceGPT->v.cap[baseoff + off];
      // merge the perms with newGPTCap
      if (cur.type != ct_Null) {
	newGPTCap.restr &= (cur.restr | CAP_RESTR_OP);
      }
      // end perms
      newGPT->v.cap[off] = cur;
    }

    caps.push_back(newGPTCap);
  }

  // Install the capabilities generated by the loop into the GPT.
  for (size_t j = 0; j < slotMax; j++)
    spaceGPT->v.cap[j] = caps[j];
  spaceGPT->v.l2v = new_l2v;

  return;
}

/**
 * @brief Find the slot and remaining offset to install a capability
 * at offset @p offset into the address space @p space.  The slot must
 * be at least @p l2arg in size.
 *
 * Return the slot and remaining offset to install a capability into
 * @p space at @p offset, with a slot size of at least @p l2arg,
 * allocating any necessary objects from @p bank, and adjusting the
 * permissions along the way so that the restrictions in @p restr,
 * if cleared, are also cleared along the path.
 *
 * For example, if you want a writable slot to be writable after it's
 * inserted, you have to clear the <tt>CAP_RESTR_RO</tt> bit of every
 * GPT along the path to the slot.
 */
struct CoyImage::TraverseRet
CoyImage::traverse_to_slot(capability bank, capability &space,
			   coyaddr_t offset, size_t l2arg,
			   uint32_t restr)
{
  if (l2arg < target.pagel2v || l2arg > bitsizeof(coyaddr_t))
    THROW(excpt::BadValue, "l2arg out of bounds");

  // we could truncate instead of throwing
  if (lowbits(offset, l2arg) != 0)
    THROW(excpt::BadValue, "offset is not l2arg-aligned");

  // Simple case:  if the space is empty, replace it.
  if (space.type == ct_Null)
    return TraverseRet(space, offset);

  // Only memCaps and Windows are allowed in the space.
  if (!cap_isMemory(space))
    THROW(excpt::BadValue, "space has incorrect type");

  size_t l2s = max(l2arg, space.u1.mem.l2g);

  coyaddr_t guard = cap_guard(space);

  // first, find out if the guard and offset are compatible.  If not,
  // we need to add in a GPT at the point of difference
  
  coyaddr_t mismatch = hibits(offset ^ guard, l2s);

  if (mismatch != 0) {
    size_t base_l2 = space.u1.mem.l2g;

    // we want the new GPT's l2g to be a multiple of
    // GPT_SLOT_INDEX_BITS above the existing space. 

    size_t l2g_desired = base_l2 +
      round_up(l2offset(mismatch) - base_l2, GPT_SLOT_INDEX_BITS);

    size_t new_l2g = min(l2g_desired, bitsizeof(coyaddr_t));
    size_t new_l2v = max(new_l2g - GPT_SLOT_INDEX_BITS, l2s);

    xassert(hibits(offset ^ guard, new_l2g) == 0);
    xassert(new_l2g > new_l2v);

    capability newGPTCap = AllocGPT(bank);
    GCPtr<CiGPT> newGPT = GetGPT(newGPTCap);

    // apply the current permissions and our desired perms to the
    // new cap
    newGPTCap.restr = restr & (space.restr | CAP_RESTR_OP);
    // end perms

    // we rely on MakeGuardedSubspace to set up the guards
    newGPTCap.u1.mem.l2g = new_l2g;
    newGPT->v.l2v = new_l2v;

    size_t l2slots = new_l2g - new_l2v;
    size_t space_slot = lowbits(guard >> new_l2v, l2slots);
    size_t arg_slot = lowbits(offset >> new_l2v, l2slots);

    xassert(space_slot != arg_slot);

    // Save off the guard for the new GPT, and clear the bits we've
    // now used for the guard and slot #s in newGPT

    coyaddr_t newGuard = hibits(guard, new_l2g);
    guard = lowbits(guard, new_l2v);
    offset = lowbits(offset, new_l2v);

    // update the slot, and replace space with newGPT, suitably guarded
    space.u1.mem.match = 0;
    newGPT->v.cap[space_slot] =
      MakeGuardedSubSpace(bank, space, guard, 0);
    space = MakeGuardedSubSpace(bank, newGPTCap, newGuard, 0);

    return TraverseRet(newGPT->v.cap[arg_slot], offset);
  }

  // The guard and offset match, up to l2s.  Now, check to see if
  // we're replacing the entire space
  if (space.u1.mem.l2g <= l2arg) {
    // nothing to do;  space is entirely replaced
    return TraverseRet(space, offset);
  }

  // From this point forward, the guard has matched, and we need to
  // traverse into the capability.

  // strip off the offset bits matched by the guard
  offset = lowbits(offset, space.u1.mem.l2g);

  // We don't allow you to insert through window capabilities
  if (cap_isWindow(space))
    THROW(excpt::BadValue, "traverse_to_slot cannot walk through windows");

  if (space.type != ct_GPT)
    THROW(excpt::BadValue, "slot contains non-GPT cap");
  
  GCPtr<CiGPT> spaceGPT = GetGPT(space);

  // splitGPT will manipulate spaceGPT to make our target slot available
  split_GPT(bank, spaceGPT, offset, l2arg);
  xassert(spaceGPT->v.l2v >= l2arg);
  
  size_t l2slots = spaceGPT->l2slots();

  size_t arg_slot = offset >> spaceGPT->v.l2v;
  xassert(arg_slot < (1u << l2slots));

  if (spaceGPT->v.l2v == l2arg) {
    // the slots are of the size requested;  just return the correct slot.

    // apply our perms to space
    space.restr &= (restr | CAP_RESTR_OP);
    // end perms

    return TraverseRet(spaceGPT->v.cap[arg_slot],
		       lowbits(offset, l2arg));
  } else {
    // We need to recurse to find the actual insertion point.  We delay
    // updating permissions until the call has succeeded.
    TraverseRet ret = 
      traverse_to_slot(bank,
		       spaceGPT->v.cap[arg_slot],
		       lowbits(offset, spaceGPT->v.l2v),
		       l2arg,
		       restr);

    // apply our perms to space now that the call has succeeded
    space.restr &= (restr | CAP_RESTR_OP);
    // end perms

    return ret;
  }
    
}

capability
CoyImage::AddSubSpaceToSpace(capability bank, capability space,
			     coyaddr_t where,
			     capability subSpace, size_t l2arg)
{
  uint32_t restr;
  if (!cap_isMemory(subSpace) && subSpace.type != ct_Null) {
    THROW(excpt::BadValue, "subSpace is not a memory object");
  }
  
  if (l2arg == 0)
    l2arg = (subSpace.type == ct_Null)? target.pagel2v : subSpace.u1.mem.l2g;
  
  // ct_Null prevents all access
  if (subSpace.type == ct_Null)
    restr = CAP_RESTR_RO | CAP_RESTR_NX | CAP_RESTR_WK;
  else
    restr = (subSpace.restr & ~CAP_RESTR_OP);
  // end perms

  TraverseRet position = traverse_to_slot(bank, space, where, l2arg, restr);
  position.slot = MakeGuardedSubSpace(bank, subSpace, position.offset, l2arg);

  return space; 
}

CoyImage::LeafInfo
CoyImage::LeafAtAddr(const capability &start, coyaddr_t where)
{
  const capability *cur = &start;
  uint32_t restr;

  for (;;) {
    if (cur->type == ct_Null)
      goto notfound;

    if (!cap_isMemory(*cur))
      THROW(excpt::BadValue, "invalid capability traversed in space");

    if (cap_isWindow(*cur))
      THROW(excpt::BadValue, "cannot peer into windows");

    if (hibits_shifted(where, cur->u1.mem.l2g) != cur->u1.mem.match)
      goto notfound;

    // we're past the guard.  clear the guarded bits
    where = lowbits(where, cur->u1.mem.l2g);

    // apply the cap's permissions
    restr |= (cur->restr & ~CAP_RESTR_OP);
    if (restr & CAP_RESTR_WK)
      restr |= CAP_RESTR_RO;

    if (cur->type == ct_GPT) {
      GCPtr<CiGPT> gpt = GetGPT(*cur);
      
      coyaddr_t slot = hibits_shifted(where, gpt->v.l2v);
      if (slot >= shift_to_hibits(1, gpt->l2slots()))
	goto notfound;
      
      where = lowbits(where, gpt->v.l2v);
      cur = &gpt->v.cap[slot];
      continue;
    }
  
    if (cur->type == ct_Page || cur->type == ct_CapPage) {
      break;
    }

    THROW(excpt::BadValue, "shouldn't get here");
  }
  
  return LeafInfo(*cur, where, restr);

 notfound:
  return LeafInfo(CiCap(), 0, restr);
}
