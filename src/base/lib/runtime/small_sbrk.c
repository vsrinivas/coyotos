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

/* Simulated sbrk */

#include <stdint.h>
#include <sys/types.h>
#include <errno.h>

#include <coyotos/machine/pagesize.h>
#include <idl/coyotos/SpaceBank.h>
#include <idl/coyotos/GPT.h>
#include <idl/coyotos/Process.h>

#include "coyotos/runtime.h"
#include "coyotos/capability_stack.h"

#define PAGE_ALIGN(x) ((uintptr_t)(x) & ~(uintptr_t)COYOTOS_PAGE_ADDR_MASK)

/* n must be < 4 */
#define CR_UNSTABLE(n)	CR_APP(CRN_LASTAPP_STABLE - CRN_FIRSTAPP + 1 + (n))

#define CR_NEWPAGE	CR_UNSTABLE(0)
#define CR_TMP1		CR_UNSTABLE(1)
#define CR_TMP2		CR_UNSTABLE(2)
#define CR_TMP3		CR_UNSTABLE(3)

static IDL_Environment IDL_E = {
  .replyCap = CR_REPLYEPT,
  .epID = 0ULL
};

extern unsigned _end;		/* lie */
static uintptr_t heap_ptr = (uintptr_t) &_end;

static bool insert_page(uintptr_t addr, 
			caploc_t newpage, 
			caploc_t tmp1, 
			caploc_t tmp2, 
			caploc_t tmp3);

static bool alloc_cap(coyotos_Range_obType type, caploc_t out);

static void free_cap(caploc_t cap);

caddr_t
sbrk(intptr_t nbytes)
{
  uintptr_t base = heap_ptr;
  uintptr_t new_heap = heap_ptr + nbytes;
  if (new_heap < base) {
    errno = ENOMEM;
    return (caddr_t)-1;
  }
    
  if (PAGE_ALIGN(base - 1) != PAGE_ALIGN(new_heap - 1)) {
    uintptr_t page = PAGE_ALIGN(base - 1) + COYOTOS_PAGE_SIZE;
    uintptr_t lastPage = PAGE_ALIGN(new_heap - 1);

    if (!capability_canPush(4)) {
      errno = ENOMEM;
      return (caddr_t)-1;
    }
    capability_push(CR_NEWPAGE);
    capability_push(CR_TMP1);
    capability_push(CR_TMP2);
    capability_push(CR_TMP3);

    for (; page != 0 && page <= lastPage; page += COYOTOS_PAGE_SIZE) {
      if (!alloc_cap(coyotos_Range_obType_otPage, CR_NEWPAGE))
	goto fail_pop;

      if (!insert_page(page, CR_NEWPAGE, CR_TMP1, CR_TMP2, CR_TMP3)) {
	free_cap(CR_NEWPAGE);
	goto fail_pop;
      }
    }
    capability_pop(CR_TMP3);
    capability_pop(CR_TMP2);
    capability_pop(CR_TMP1);
    capability_pop(CR_NEWPAGE);
  }
  heap_ptr = new_heap;
  return (caddr_t)base;

 fail_pop:
  capability_pop(CR_TMP3);
  capability_pop(CR_TMP2);
  capability_pop(CR_TMP1);
  capability_pop(CR_NEWPAGE);
  errno = ENOMEM;
  return (caddr_t)-1;
}

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

static bool 
insert_page(uintptr_t addr, 
	    caploc_t pageCap, caploc_t cap, caploc_t next, caploc_t spare)
{
  /*
   * We will loop as we go down;  the capability registers used looks like:
   *      cap        next     spare    next_spare
   *  1.  TMP1       TMP2     TMP3     TMP1
   *  2.  TMP2       TMP3     TMP1     TMP2
   *  3.  TMP3       TMP1     TMP2     TMP3
   *  4.  TMP1       TMP2     TMP3     TMP1
   *  ... etc ...
   */

  caploc_t next_spare = cap;

  if (!coyotos_Process_getSlot(CR_SELF, coyotos_Process_cslot_addrSpace,
			       cap, &IDL_E))
    return false;

  /* check the current address space to see if it is large enough to contain
   * the requested address.  If not, we need to add a new top-level GPT to
   * the address space.
   *
   * For now, we always place a full-sized GPT at the top;  if we wanted
   * to be more stingy, we could grow it more slowly.
   */
  {
    guard_t theGuard;
    if (!coyotos_Memory_getGuard(cap, &theGuard, &IDL_E))
      return false;    /* shouldn't happen */

    uint32_t match = guard_match(theGuard);
    if (match != 0)
      return false;

    coyotos_Memory_l2value_t l2g = guard_l2g(theGuard);
    /* check to see that the address space is well-formed */
    if (match != 0 || l2g > COYOTOS_HW_ADDRESS_BITS)
      return (false);
    
    uintptr_t matchbits = highbits_shifted(addr, l2g);
    if (matchbits != 0) {
      // we need to add a GPT
      if (!alloc_cap(coyotos_Range_obType_otGPT, spare))
	return false;

      l2g = COYOTOS_HW_ADDRESS_BITS;
      coyotos_Memory_l2value_t l2v = l2g - coyotos_GPT_l2slots;
      coyotos_Memory_l2value_t l2v_ignored = 0;
      
      if (!coyotos_GPT_setl2v(spare, l2v, &l2v_ignored, &IDL_E) ||
	  !coyotos_Memory_setGuard(spare, make_guard(0, l2g), spare, &IDL_E) ||
	  !coyotos_AddressSpace_setSlot(spare, 0, cap, &IDL_E) ||
	  !coyotos_Process_setSlot(CR_SELF, coyotos_Process_cslot_addrSpace,
				   spare, &IDL_E)) {
	free_cap(spare);
	return false;
      }
    }
  }


  coyotos_Memory_l2value_t l2v = 0;
  coyotos_Memory_l2value_t lastl2v = COYOTOS_SOFTADDR_BITS;

  for (;;) {
    if (!coyotos_GPT_getl2v(cap, &l2v, &IDL_E))
      return false;

    uintptr_t slot = highbits_shifted(addr, l2v);
    uintptr_t remaddr = lowbits(addr, l2v);
    
    if (l2v == COYOTOS_PAGE_ADDR_BITS) {
      if (!coyotos_AddressSpace_setSlot(cap, slot, pageCap, &IDL_E))
	return false;
    }
    if (!coyotos_AddressSpace_getSlot(cap, slot, next, &IDL_E))
      return false;

    guard_t theGuard;
    if (!coyotos_Memory_getGuard(next, &theGuard, &IDL_E)) {
      if (remaddr != 0)
	return false;
      if (!coyotos_AddressSpace_setSlot(cap, slot, pageCap, &IDL_E))
	return false;
      return true;
    }

    uint32_t match = guard_match(theGuard);
    coyotos_Memory_l2value_t l2g = guard_l2g(theGuard);

    /* check to see that the address space is well-formed */
    if (match != 0 || l2g > COYOTOS_HW_ADDRESS_BITS || l2g > l2v)
      return (false);
    
    uintptr_t matchbits = highbits_shifted(remaddr, l2g);
    if (matchbits != 0) {
      // we need to add a GPT
      if (!alloc_cap(coyotos_Range_obType_otGPT, spare))
	return false;

      if (!coyotos_GPT_setl2v(spare, l2g, &l2v, &IDL_E) ||
	  !coyotos_Memory_setGuard(spare, 
				   make_guard(0, 
					      l2g + 
					      coyotos_GPT_l2slots),
				   spare, &IDL_E) ||
	  !coyotos_Memory_setGuard(next, 
				   make_guard(0, l2g),
				   next, &IDL_E) ||
	  !coyotos_AddressSpace_setSlot(spare, 0, next, &IDL_E) ||
	  !coyotos_AddressSpace_setSlot(cap, slot, spare, &IDL_E)) {
	free_cap(spare);
	return false;
      }
      continue;  // re-execute loop with newly inserted GPT
    }
    if (l2g == COYOTOS_PAGE_ADDR_BITS) {
      // replacing a page.  Just overwrite it.
      if (!coyotos_AddressSpace_setSlot(cap, slot, pageCap, &IDL_E))
	return false;
      return true;
    }

    if (l2v >= lastl2v)
      return false;
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

static bool 
alloc_cap(coyotos_Range_obType type, caploc_t cap)
{
  return coyotos_SpaceBank_alloc(CR_SPACEBANK,
				 type,
				 coyotos_Range_obType_otInvalid,
				 coyotos_Range_obType_otInvalid,
				 cap,
				 CR_NULL,
				 CR_NULL,
				 &IDL_E);
}

static void
free_cap(caploc_t cap)
{
  (void) coyotos_SpaceBank_free(CR_SPACEBANK, 1, 
				cap, CR_NULL, CR_NULL, &IDL_E);
}

