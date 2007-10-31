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
#include "coyotos/captemp.h"

#define PAGE_ALIGN(x) ((uintptr_t)(x) & ~(uintptr_t)COYOTOS_PAGE_ADDR_MASK)

/* n must be < 4 */
#define CR_UNSTABLE(n)	CR_APP(CRN_LASTAPP_STABLE - CRN_FIRSTAPP + 1 + (n))

#define CR_NEWPAGE	CR_UNSTABLE(0)
#define CR_TMP1		CR_UNSTABLE(1)
#define CR_TMP2		CR_UNSTABLE(2)
#define CR_TMP3		CR_UNSTABLE(3)

extern unsigned _end;		/* lie */
static uintptr_t heap_ptr = (uintptr_t) &_end;
static uintptr_t max_heap = (uintptr_t) &_end;

static bool insert_page(uintptr_t addr, caploc_t newpage, caploc_t tmp1);

static bool alloc_cap(coyotos_Range_obType type, caploc_t out);

static void free_cap(caploc_t cap);

caddr_t
sbrk(intptr_t nbytes)
{
  uintptr_t base = heap_ptr;
  uintptr_t new_heap = heap_ptr + nbytes;

  if (new_heap <= max_heap) {
    /* can't move sbrk below _end */
    if (new_heap < (uintptr_t) &_end)
      goto fail;

  } else if (PAGE_ALIGN(base - 1) != PAGE_ALIGN(new_heap - 1)) {
    uintptr_t page = PAGE_ALIGN(base - 1) + COYOTOS_PAGE_SIZE;
    uintptr_t lastPage = PAGE_ALIGN(new_heap - 1);

    caploc_t newpage = captemp_alloc();
    caploc_t tmp = captemp_alloc();

    bool failed = false;

    for (; page != 0 && page <= lastPage; page += COYOTOS_PAGE_SIZE) {
      if (!alloc_cap(coyotos_Range_obType_otPage, newpage)) {
        failed = true;
        break;
      }

      if (!insert_page(page, newpage, tmp)) {
	free_cap(newpage);
        failed = true;
        break;
      }
    }
    captemp_release(tmp);
    captemp_release(newpage);
    if (failed)
      goto fail;
  }
  heap_ptr = new_heap;

  if (heap_ptr > max_heap)
    max_heap = heap_ptr;

  return (caddr_t)base;

 fail:
  errno = ENOMEM;
  return (caddr_t)-1;
}

static bool 
insert_page(uintptr_t addr, caploc_t pageCap, caploc_t cap)
{
  if (!coyotos_Process_getSlot(CR_SELF, coyotos_Process_cslot_addrSpace,
			       cap))
    return false;

  /* make sure the guard is kosher, and l2v is as expected */
  guard_t theGuard;
  if (!coyotos_Memory_getGuard(cap, &theGuard))
    return false;    /* shouldn't happen */

  uint32_t match = guard_match(theGuard);
  if (match != 0)
    return false;

  coyotos_Memory_l2value_t l2g = guard_l2g(theGuard);
  if (l2g != COYOTOS_PAGE_ADDR_BITS + coyotos_GPT_l2slots)
    return false;

  uintptr_t matchbits = (addr >> l2g);
  if (matchbits != 0)
    return false;

  coyotos_Memory_l2value_t l2v = 0;
  if (!coyotos_GPT_getl2v(cap, &l2v))
    return false;

  if (l2v != COYOTOS_PAGE_ADDR_BITS)
    return false;

  uintptr_t slot = (addr >> l2v);

  if (!coyotos_AddressSpace_setSlot(cap, slot, pageCap))
    return false;

  return true;
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
				 CR_NULL);
}

static void
free_cap(caploc_t cap)
{
  (void) coyotos_SpaceBank_free(CR_SPACEBANK, 1, cap, CR_NULL, CR_NULL);
}

