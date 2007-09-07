/*
 * Copyright (C) 2007, The EROS Group, LLC
 *
 * This file is part of the EROS Operating System.
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
 * @brief SpaceBank alloc-only memory allocation implementation
 */

#include "SpaceBank.h"

uintptr_t mybrk = 0;  /* allocation base */
uintptr_t myend = 0;  /* after allocations are complete, end of region */
const struct CoyImgHdr *image_header;

uint32_t addrspace_l2v = (COYOTOS_HW_ADDRESS_BITS - coyotos_GPT_l2slots);

void alloc_init(void)
{
  uint32_t oldl2v;

  // Our mkimage has an initial GPT which we use as the top of our 
  // address space.
  MUST_SUCCEED(coyotos_Memory_setGuard(CR_INITGPT, 
				       make_guard(0, COYOTOS_HW_ADDRESS_BITS),
				       CR_ADDRSPACE,
				       IDL_E));
  MUST_SUCCEED(coyotos_GPT_setl2v(CR_ADDRSPACE, 
				  addrspace_l2v, 
				  &oldl2v, 
				  IDL_E));
  
  // Put the existing address space in slot 0 of the initial GPT
  MUST_SUCCEED(coyotos_Process_getSlot(CR_SELF, 
				       coyotos_Process_cslot_addrSpace,
				       CR_TMP1, 
				       IDL_E));

  MUST_SUCCEED(coyotos_AddressSpace_setSlot(CR_ADDRSPACE, 
					    0, 
					    CR_TMP1,
					    IDL_E));

  // And install the new address space
  MUST_SUCCEED(coyotos_Process_setSlot(CR_SELF, 
				       coyotos_Process_cslot_addrSpace,
				       CR_ADDRSPACE, 
				       IDL_E));

  MUST_SUCCEED(coyotos_GPT_getl2v(CR_ADDRSPACE, &addrspace_l2v, IDL_E));

  /* Set up a read-only Page zero mapping */
  get_pagecap(CR_TMP1, 0);
  MUST_SUCCEED(coyotos_Memory_reduce(CR_TMP1, 
				     coyotos_Memory_restrictions_readOnly,
				     CR_TMP1,
				     IDL_E));

  /* And install it as the first page in the break */
  MUST_SUCCEED(coyotos_AddressSpace_setSlot(CR_ADDRSPACE, 
					    1,
					    CR_TMP1, 
					    IDL_E));

  /* Now, set up our break state to match. */
  mybrk = ((uintptr_t)1 << addrspace_l2v);
  image_header = (const struct CoyImgHdr *)mybrk;

  mybrk += COYOTOS_PAGE_SIZE;
}

#define MINALIGN (sizeof (oid_t))

static inline uintptr_t highbits_shifted(uintptr_t addr, uint8_t shift)
{
  if (shift >= 32)
    return 0;
  return (addr >> shift);
}

static inline uintptr_t lowbits(uintptr_t addr, uint8_t shift)
{
  if (shift >= 32)
    return addr;
  return addr & ((1UL << shift) - 1);
}

static void
install_Page(caploc_t pageCap, uintptr_t addr)
{
  /*
   * We will loop as we go down;  the capability registers used looks like:
   *      cap        next     spare    next_spare
   *  1.  ADDRSPACE  TMP1     TMP2     TMP3
   *  2.  TMP1       TMP2     TMP3     TMP1
   *  3.  TMP2       TMP3     TMP1     TMP2
   *  4.  TMP3       TMP1     TMP2     TMP3
   *  5.  TMP1       TMP2     TMP3     TMP1
   *  ... etc. ...
   */
  assert(pageCap.raw != CR_TMP1.raw && pageCap.raw != CR_TMP2.raw
	 && pageCap.raw != CR_TMP3.raw);

  caploc_t cap = CR_ADDRSPACE;
  caploc_t next = CR_TMP1;
  caploc_t spare = CR_TMP2;
  caploc_t next_spare = CR_TMP3;

  {
    guard_t theGuard = 0;
    
    MUST_SUCCEED(coyotos_Memory_getGuard(pageCap,
					 &theGuard, IDL_E));
    assert3(guard_match(theGuard), ==, 0);
    assert3(guard_l2g(theGuard), ==, COYOTOS_PAGE_ADDR_BITS);
  }

  coyotos_Memory_l2value_t l2v = 0;
  coyotos_Memory_l2value_t lastl2v = COYOTOS_SOFTADDR_BITS;

  // the top-level GPT always has a guard of zero and an l2g of the HW
  // address limit
  for (;;) {
    MUST_SUCCEED(coyotos_GPT_getl2v(cap, &l2v, IDL_E));

    uintptr_t slot = highbits_shifted(addr, l2v);
    uintptr_t remaddr = lowbits(addr, l2v);
    
    if (l2v == COYOTOS_PAGE_ADDR_BITS) {
      MUST_SUCCEED(coyotos_AddressSpace_setSlot(cap, slot, pageCap, IDL_E));
      return;
    }
    MUST_SUCCEED(coyotos_AddressSpace_getSlot(cap, slot, next, IDL_E));

    guard_t theGuard;
    if (!coyotos_Memory_getGuard(next, &theGuard, IDL_E)) {
      assert3(remaddr, ==, 0);
      MUST_SUCCEED(coyotos_AddressSpace_setSlot(cap, slot, pageCap, IDL_E));
      return;
    }

    uint32_t match = guard_match(theGuard);
    coyotos_Memory_l2value_t l2g = guard_l2g(theGuard);
    assert3(match, ==, 0);
    assert3(l2g, <=, 32);
    assert3(l2g, <=, l2v);
    
    uintptr_t matchbits = highbits_shifted(remaddr, l2g);
    if (matchbits != 0) {
      // we need to add a GPT
      require_GPT(spare);
      MUST_SUCCEED(coyotos_GPT_setl2v(spare, l2g, &l2v, IDL_E));
      MUST_SUCCEED(coyotos_Memory_setGuard(spare, 
					   make_guard(0, 
						      l2g + 
						      coyotos_GPT_l2slots),
					   spare, IDL_E));
      MUST_SUCCEED(coyotos_Memory_setGuard(next, 
					   make_guard(0, l2g),
					   next, IDL_E));
      MUST_SUCCEED(coyotos_AddressSpace_setSlot(spare, 0, next, IDL_E));
      MUST_SUCCEED(coyotos_AddressSpace_setSlot(cap, slot, spare, IDL_E));
      continue;  // re-execute loop with newly inserted GPT
    }
    if (l2g == COYOTOS_PAGE_ADDR_BITS) {
      // replacing a page.  Just overwrite it.
      MUST_SUCCEED(coyotos_AddressSpace_setSlot(cap, slot, pageCap, IDL_E));
      return;
    }

    assert3(l2v, <, lastl2v);
    lastl2v = l2v;

    // we're traversing *into* a GPT.  Since the guard is zero, and
    // we match it, we just need to update the address and swap around
    // our caps.
    addr = remaddr;

    cap = next;
    next = spare;
    spare = next_spare;
    next_spare = cap;
  }
}

/** @brief align a size up to the minimum alignment boundry.
 */  

static inline uintptr_t
align_up(uintptr_t x)
{
  return x + ((-x) & MINALIGN);
}

static inline uintptr_t
pagealign_up(uintptr_t x)
{
  return x + ((-x) & COYOTOS_PAGE_ADDR_MASK);
}

void *
allocate_bytes(size_t bytes_arg)
{
  assert3(mybrk, !=, 0);

  size_t bytes = align_up(bytes_arg);
  const uintptr_t a_start = mybrk;
  const uintptr_t a_end = mybrk + bytes;

  uintptr_t pos;

  assert3(a_end, >=, mybrk);

  for (pos = pagealign_up(mybrk);
       pos < a_end;
       pos += COYOTOS_PAGE_SIZE) {
    assert3(pos, >=, mybrk);

    require_Page(CR_TMPPAGE);
    install_Page(CR_TMPPAGE, pos);
  }

  assert3(a_end, >=, mybrk);
  mybrk = a_end;
  return (void *)a_start;
}

void *
map_pages(oid_t base, oid_t bound)
{
  assert3(mybrk, !=, 0);

  size_t nPages = bound - base;

  const uintptr_t start = pagealign_up(mybrk);
  const uintptr_t end = start + COYOTOS_PAGE_SIZE * nPages;

  size_t idx;

  for (idx = 0; idx < nPages; idx++) {
    get_pagecap(CR_TMPPAGE, base + idx);
    install_Page(CR_TMPPAGE, start + idx * COYOTOS_PAGE_SIZE);
  }
  mybrk = end;
  return (void *)start;
}

void
alloc_finish(void)
{
  myend = mybrk;
  mybrk = 0;
}
