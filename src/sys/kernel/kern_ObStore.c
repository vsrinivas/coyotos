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
 * @brief Object store interface from kernel perspective.
 */

#include <kerninc/ObStore.h>
#include <kerninc/ObjectHash.h>
#include <kerninc/Cache.h>
#include <kerninc/PhysMem.h>
#include <kerninc/assert.h>
#include <kerninc/printf.h>
#include <idl/coyotos/Range.h>

ObjectHeader*
obstore_require_object(ObType ty, oid_t oid, bool waitForRange, HoldInfo *hi)
{
  HoldInfo hashMutex = obhash_grabMutex(ty, oid);
  ObjectHeader *obHdr = obhash_lookup(ty, oid, false, hi);

  if (obHdr == 0 
      && oid >= coyotos_Range_physOidStart 
      && ty == ot_Page) {
    /* We have been asked for a physical page frame. There are three
     * possibilities here. */

    /* Case 1:. The PA matches an existing object page frame. Evict the
     * occupant, re-tag that frame, and return it.
     */

    kpa_t pa = (oid - coyotos_Range_physOidStart) * COYOTOS_PAGE_SIZE;

    /* get_PhysPage returns locked object header. */
    Page *page = cache_get_physPage(pa);
    if (page)
      return &page->mhdr.hdr;

    /* Case 2: The PA falls within a device memory or ROM
     * region. Allocate a new object header from the ObHdr free list
     * and assign it to the frame.
     */

#if 0
    /* See if this is a ROM or a device RAM range, and whether we
       have a full page there: */
    PmemInfo *pmi = pmem_FindRegion(pa);
    if (!pmi)
      goto no_such_page;

    if (pa * COYOTOS_PAGE_SIZE > pmi->bound)
      goto no_such_page;

    switch (pmi->cls) {
    case pmc_ROM:
    case pmc_RAM:
    case pmc_NVRAM:
      break;
    default:
      goto no_such_page;
    }

    switch (pmi->use) {
    case pmu_KERNEL:
    case pmu_KHEAP:
    case pmu_KMAP:
    case pmu_PAGES:
    case pmu_PGTBL:
      goto no_such_page;

    case pmu_AVAIL:
      fatal("Unclaimed memory!\n");

    case pmu_ISLIMG:
      fatal("ISL Image not released!\n");

    default:
      break;
    }
#endif

    /** @bug Eventually we will run out and the following allocation
     * will fail. Fix this by making the heap extensible. */
    page = cache_alloc_page_header();
    if (page) {
      /** BEGIN NON-YIELDING SECTION
       *
       * The following initialization must not call anything that
       * might yield, lest we lose track of the object that is no
       * longer on the free list.
       */

      /* This can nominally yield, which would be bad, but we
       * currently have exclusive access, so we know that it will not
       * do so.
       */
      (void) mutex_grab(&page->mhdr.hdr.lock);

      /* Note that we do NOT zero device or ROM pages. These may hold
	 critical state (e.g. frame buffer) or they may be ROM pages */
      assert(page);
      page->mhdr.hdr.ty = ot_Page;
      page->mhdr.hdr.oid = oid;
      page->mhdr.hdr.allocCount = 0;
      page->mhdr.hdr.hasDiskCaps = 0;
      page->mhdr.hdr.current = 1;
      page->mhdr.hdr.snapshot = 0;
      page->mhdr.hdr.dirty = 0;
      page->mhdr.hdr.pinned = 1;
      page->mhdr.hdr.cksum = 0;	/* dirty; doesn't matter. */
      page->pa = pa;

      obhash_insert_obj(page);

      /* END NON-YIELDING SECTION */
      mutex_release(hashMutex);

      return &page->mhdr.hdr;
    }

#if 0
  no_such_page:
#endif
    /* Case 3: We don't know about this range. Either the allocate
     * fails or we block for the range to appear. This case is not yet
     * handled -- or at least, not gracefully.
     */

    fatal("Attempt to use unknown physical page 0x%llx.\n", pa);
    assert(!waitForRange);
  }

  if (!obHdr) {
    /* check that the OID is within the allowed range */
    if (oid >= Cache.max_oid[ty]) {
      assert(!waitForRange);
      return 0;
    }

    obHdr = cache_alloc(ty);
    obHdr->ty = ty;
    obHdr->oid = oid;
    obHdr->allocCount = 0;
    obHdr->current = 1;

    cache_install_new_object(obHdr);
    obhash_insert(obHdr);
  }
  assert(obHdr);

  /* Fetch object from disk */

  mutex_release(hashMutex);

  return obHdr;
}

void
obstore_write_object_back(ObjectHeader *hdr)
{
  assert(false);
}
