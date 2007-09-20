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
 * @brief Kernel memory tree walker
 */

#include <kerninc/MemWalk.h>
#include <kerninc/GPT.h>
#include <kerninc/capability.h>
#include <kerninc/assert.h>
#include <kerninc/util.h>

/** @brief Generate a mask for the low @p bits bits of a <tt>coyaddr_t</tt>.
 *
 * 0 @< bits @<= COYOTOS_SOFTADDR_BITS
 */
static inline coyaddr_t ca_bitmask(size_t bits)
{
  /*
   * We need to compute:
   *
   *   ((coyaddr_t)1 @<@< bits) - 1
   *
   * The problem is that @p bits could be COYOTOS_SOFTADDR_BITS, and
   * the C standards make shifting by the number of bits in the
   * address undefined (most hardware treats it as a shift by 0, which
   * is not what you want).  Since we require that @p bits is non-zero, we
   * get ourselves out of undefined territory by doing the following
   * transformation:
   *
   *   ((coyaddr_t)1 @<@< bits) - 1
   * = ((coyaddr_t)1 @<@< (1 + bits - 1)) - 1
   * = (((coyaddr_t)1 @<@< 1) @<@< (bits - 1)) - 1
   * = ((coyaddr_t)2 @<@< (bits - 1)) - 1
   *
   * That is, we shift *2* by *bits - 1*, which is always in range.
   */
  return (((coyaddr_t)2 << (bits - 1)) - 1);
}

/** @brief Get the low @p bits of @p addr.
 *
 * Precondition: 0 @< bits @<= COYOTOS_SOFTADDR_BITS
 */
static inline coyaddr_t 
ca_lowbits(coyaddr_t addr, size_t bits)
{
  return (addr & ca_bitmask(bits));
}

/** @brief Return @p addr with the low @p bits bits cleared.
 *
 * Precondition: 0 @< bits @<= COYOTOS_SOFTADDR_BITS
 */
static inline coyaddr_t 
ca_highbits(coyaddr_t addr, size_t bits)
{
  return (addr & ~ca_bitmask(bits));
}

coyotos_Process_FC
memwalk(capability *cap, coyaddr_t addr, bool forWrite,
	MemWalkResults *results /* OUT */)
{
  MemWalkEntry *ent = results->ents;
  size_t idx = 0;
  uint8_t cum_restr = 0;

  GPT *gpt = 0;
  GPT *bggpt = 0;

  while (idx < MEMWALK_MAX) {
    cap_prepare(cap);

    // Most of the walk will be GPTs;  to speed up that case, we
    // explicitly loop on GPTs.
    while (cap->type == ct_GPT) {
      uint8_t l2g = cap->u1.mem.l2g;
      coyaddr_t guard = (coyaddr_t)cap->u1.mem.match << l2g;
      ent[idx].guard = guard;
      ent[idx].window = 0;

      if (ca_highbits(addr, l2g) != guard)
	goto invalid_addr;
      addr ^= guard;
      ent[idx].remAddr = addr;

      gpt = (GPT *)cap->u2.prepObj.target;
      if (gpt->state.bg)
	bggpt = gpt;
      
      coyaddr_t slot = addr >> gpt->state.l2v;
      if (slot >= gpt_addressable_slots(gpt))
	goto invalid_addr;

      addr -= slot << gpt->state.l2v;

      ent[idx].l2g = min(l2g, gpt_effective_l2g(gpt));
      ent[idx].entry = (MemHeader *)gpt;
      ent[idx].slot = slot;
      ent[idx].restr = cap->restr;
      ent[idx].l2v = gpt->state.l2v;
      cum_restr |= cap->restr;
      idx++;

      if (idx >= MEMWALK_MAX)
	goto malformed_space;		// too many traversals

      cap = &gpt->state.cap[slot];
      cap_prepare(cap);
    }

    // The capability may not actually be a memory object, but doing this
    // here allows the code below to be a little more generic.
    size_t l2g = cap->u1.mem.l2g;
    coyaddr_t guard = (coyaddr_t)cap->u1.mem.match << l2g;
    ent[idx].guard = guard;
    ent[idx].l2g = l2g;
    ent[idx].l2v = l2g;
    if (ca_highbits(addr, l2g) != guard) {
      switch (cap->type) {
      case ct_GPT:
      case ct_Page:
      case ct_CapPage:
      case ct_Window:
      case ct_LocalWindow:
      case ct_Null:
	goto invalid_addr;

      default:
	goto malformed_space;
      }
    }
    addr ^= guard;
    ent[idx].remAddr = addr;

    switch (cap->type) {
    case ct_Page:
    case ct_CapPage:
      ent[idx].window = 0;
      ent[idx].entry = (MemHeader *)cap->u2.prepObj.target;
      ent[idx].slot = 0;
      ent[idx].l2v = 0;
      ent[idx].restr = cap->restr;
      cum_restr |= cap->restr;
      idx++;
      goto success;

    case ct_LocalWindow: {
      uint32_t slot = cap->allocCount;  // "slot" to go through
      addr |= cap->u2.offset;

      /// @bug slot check should be a capability invariant
      assert(slot < NUM_GPT_SLOTS);
      if (gpt == 0)
	goto malformed_space;

      capability *ncap = &gpt->state.cap[slot];
      if (ncap->type == ct_Window || ncap->type == ct_LocalWindow)
	goto malformed_space;

      ent[idx].window = 1;
      ent[idx].entry = &gpt->mhdr;
      ent[idx].slot = slot;
      ent[idx].restr = cap->restr;
      cum_restr |= cap->restr;
      idx++;

      cap = ncap;
      continue; 
    }
    case ct_Window: {
      addr |= cap->u2.offset;
      if (bggpt == 0)
	goto malformed_space;			   // no background GPT

      capability *ncap = &bggpt->state.cap[GPT_BACKGROUND_SLOT];
      if (ncap->type == ct_Window || ncap->type == ct_LocalWindow)
	goto malformed_space;

      // Update gpt to be the last GPT traversed
      gpt = bggpt;

      ent[idx].window = 1;
      ent[idx].entry = &bggpt->mhdr;
      ent[idx].slot = MEMWALK_SLOT_BACKGROUND;
      ent[idx].restr = cap->restr;
      cum_restr |= cap->restr;
      idx++;

      cap = ncap;

      continue;
    }
    case ct_GPT:
      assert(0);
      goto malformed_space;

    case ct_Null:
      goto invalid_addr;

    default:
      goto malformed_space;

    }
  }
  /* must be malformed;  no breaks allowed */
  assert(idx == MEMWALK_MAX);

  /*
   * Return points for various fault conditions.  All returns from
   * this procedure should go through these labels.
   */
 malformed_space:
  results->cum_restr = cum_restr;
  results->count = idx;

  if (forWrite && (cum_restr & (CAP_RESTR_RO|CAP_RESTR_WK)))
    return coyotos_Process_FC_AccessViolation;

  return coyotos_Process_FC_MalformedSpace;  // too many traversals

 invalid_addr:
  results->cum_restr = cum_restr;
  results->count = idx;

  if (forWrite && (cum_restr & (CAP_RESTR_RO|CAP_RESTR_WK)))
    return coyotos_Process_FC_AccessViolation;

  return coyotos_Process_FC_InvalidDataReference;

 success:
  results->cum_restr = cum_restr;
  results->count = idx;

  if (forWrite && (cum_restr & (CAP_RESTR_RO|CAP_RESTR_WK)))
    return coyotos_Process_FC_AccessViolation;

  return 0;
}

/**
 * @brief support routine for extended fetch/store.  Interface is identical
 * to @p memwalk, with the addition of the @p l2stop field.
 *
 * 
 */
coyotos_Process_FC
extended_memwalk(capability *cap, coyaddr_t addr, uint8_t l2stop, 
		 bool forWrite, MemWalkResults *results /* OUT */)
{
  coyotos_Process_FC fc = memwalk(cap, addr, 0, results);

  size_t cur_end = 0;
  size_t i;

  for (i = 0; i < results->count; i++) {
    MemWalkEntry *e = &results->ents[i];
    if (e->l2g < l2stop)
      break;

    if (e->window)
      continue;

    if (e->entry->hdr.ty == ot_GPT) {
      if (e->l2v < l2stop)
	break;

      // This GPT is one we may want to return; update the end pointer
      cur_end = i + 1;
      if (e->l2v == l2stop)
	break;
    }
  }
  // if we broke out early, there is no fault to report.
  if (i < results->count)
    fc = 0;

  /* Compute the new cumulative restrictions */
  uint8_t cum_restr = 0;
  for (i = 0; i < cur_end; i++)
    cum_restr |= results->ents[i].restr;

  if (forWrite && (cum_restr & (CAP_RESTR_RO|CAP_RESTR_WK)))
    fc = coyotos_Process_FC_AccessViolation;

  results->count = cur_end;
  results->cum_restr = cum_restr;
  return (fc);
}
