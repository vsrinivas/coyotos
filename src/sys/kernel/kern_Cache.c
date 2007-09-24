/*
 * Copyright (C) 2007, The EROS Group, LLC
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
 * @brief Cache Management.
 *
 * Kernel caches can broadly be classified into two types: object
 * caches and supporting data structures. Supporting data structures
 * are the various depend table entries and the PrepHeader structures.
 *
 * kern_Cache.c manages the allocation and cleaning of both types.
 */

#include <hal/transmap.h>

#include <kerninc/assert.h>
#include <kerninc/string.h>
#include <kerninc/printf.h>
#include <kerninc/util.h>
#include <kerninc/PhysMem.h>
#include <kerninc/Cache.h>
#include <kerninc/Process.h>
#include <kerninc/GPT.h>
#include <kerninc/Endpoint.h>
#include <kerninc/StallQueue.h>
#include <kerninc/Depend.h>
#include <kerninc/malloc.h>
#include <kerninc/string.h>
#include <kerninc/pstring.h>
#include <kerninc/shellsort.h>
#include <kerninc/ObjectHash.h>
#include <kerninc/ReadyQueue.h>
#include <kerninc/FreeList.h>

#include <idl/coyotos/Range.h>

#include <obstore/CoyImgHdr.h>
#include <coyotos/endian.h>

#include <obstore/CoyImgHdr.h>

#define DEBUG_CACHE if (0)

enum {
  // upgrade when 1/2 way down the list
  UPGRADE_AGE_FRACTION = 2,
  AGING_PER_ALLOC = 2,  // process two objects for every 1 allocated
};

/* 
 * At link time, set up the obCache array to point to the per-type Frame 
 * Caches.
 *
 * We set up aliases to point at their base frametype's cache.
 */
Cache_s Cache = { 
  {
#define DEFFRAME(ft, val) \
    &Cache.c_ ## ft,
#define ALIASFRAME(alias_ft, ft, val) \
    &Cache.c_ ## ft,
#define NODEFFRAME(ft, val)
#include <kerninc/frametype.def>
  } 
};

void
cache_estimate_sizes(size_t pagesPerProc, kpsize_t totPage)
{
  // These are completely unmotivated guesses.
  enum { NGPT_PER_PROC = 6,
	 NENDPT_PER_PROC = 8,
	 NDEPEND_PER_PAGE = 3,	/* VERY architecture sensitive! */
	 NPTE_PER_PAGE = 3,	/* VERY architecture sensitive! */
  };

  DEBUG_CACHE printf("Total allocatable bytes: 0x%016llx\n",
		     pmem_Available(&pmem_need_bytes, 1, false));

  Cache.c_Page.count = totPage;

  DEBUG_CACHE printf("Total allocatable pages: %d\n", Cache.c_Page.count);

  if (pagesPerProc == 0)
    pagesPerProc = 25; /* rough guess for bringup */

#define TWEAK(cache, size)						\
  do {									\
    size_t nBytes = (cache)->count * (size);				\
    nBytes = align_up(nBytes, COYOTOS_PAGE_SIZE);			\
    (cache)->count = (nBytes / (size));					\
  } while(0);
  
#define TWEAK_OBJ(type)		TWEAK(&Cache.c_ ## type, sizeof (type))
#define TWEAK_CACHE(cache)	TWEAK(&Cache.cache, sizeof (*Cache.cache.vec))
  
#define ADJUST(cache, size)						\
  do {									\
    size_t nBytes = (cache)->count * (size);				\
    size_t cachePages = align_up(nBytes, COYOTOS_PAGE_SIZE);		\
    cachePages /= COYOTOS_PAGE_SIZE;					\
    Cache.c_Page.count -= cachePages;					\
  } while(0)

#define ADJUST_OBJ(type)	ADJUST(&Cache.c_ ## type, sizeof (type))
#define ADJUST_PAGE(type) \
  ADJUST(&Cache.c_ ## type, sizeof (type) + sizeof (void *))
#define ADJUST_CACHE(cache)	ADJUST(&Cache.cache, sizeof (*Cache.cache.vec))

  if (Cache.c_Process.count == 0) {
    Cache.c_Process.count = Cache.c_Page.count / pagesPerProc;
    TWEAK_OBJ(Process);
  }
  ADJUST_OBJ(Process);

  if (Cache.c_GPT.count == 0) {
    Cache.c_GPT.count = Cache.c_Process.count * NGPT_PER_PROC;
    TWEAK_OBJ(GPT);
  }
  ADJUST_OBJ(GPT);
  
  if (Cache.c_Endpoint.count == 0) {
    Cache.c_Endpoint.count = Cache.c_Process.count * NENDPT_PER_PROC;
    TWEAK_OBJ(Endpoint);
  }
  ADJUST_OBJ(Endpoint);
  
  // Based on space that is left, estimate size of depend structures:
  if (Cache.dep.count == 0) {
    Cache.dep.count = Cache.c_Page.count * NDEPEND_PER_PAGE;
    TWEAK_CACHE(dep);
  }
  ADJUST_CACHE(dep);

  if (Cache.rmap.count == 0) {
    Cache.rmap.count = Cache.c_Page.count * NPTE_PER_PAGE;
    TWEAK_CACHE(rmap);
  }
  ADJUST_CACHE(rmap);

  Cache.ote.count =
    Cache.c_Process.count
    + Cache.c_GPT.count
    + Cache.c_Endpoint.count
    + Cache.c_Page.count
    ;

  // Add half again to allow for GC:
  Cache.ote.count += (Cache.ote.count/2);
  TWEAK_CACHE(ote);
  ADJUST_CACHE(ote);

  // Add some fuzz to the page total, because the various align_ups
  // that we have done above can lead to a small number of free pages
  // that we will not have properly accounted for.
  Cache.c_Page.count += 4;

  // Account for per-page structures:
  ADJUST_OBJ(Page);			/* deals with page HEADERS */
  
#undef TWEAK
#undef TWEAK_OBJ
#undef TWEAK_CACHE
#undef ADJUST
#undef ADJUST_OBJ
#undef ADJUST_CACHE
  printf("nProc %d ngpt %d nendpt %d ndep %d nrm %d nOTE %d npage %d\n",
	 Cache.c_Process.count,
	 Cache.c_GPT.count,
	 Cache.c_Endpoint.count,
	 Cache.dep.count,
	 Cache.rmap.count,
	 Cache.ote.count,
	 Cache.c_Page.count);
}

/** @brief Set up the watermarks for a frame cache.  
 *
 * If @p nObj is non-zero, it overrides the count of objects in the cache
 * for the purposes of determining the watermarks.
 */
static inline void
cache_setup_watermarks(ObFrameCache *cache, size_t nObj)
{
  enum {
    RECLAIM_HIGH_FRACTION = 7,
    RECLAIM_LOW_FRACTION = 20,

    CHECK_HIGH_FRACTION = 10,
    CHECK_LOW_FRACTION = 30
  };

  if (nObj == 0)
    nObj = cache->count;

  assert(nObj <= cache->count);
  
  cache->active.lowWater = 0;
  cache->active.hiWater = 0;

  cache->check.lowWater = max(nObj / CHECK_LOW_FRACTION, 1);
  cache->check.hiWater = max(nObj / CHECK_HIGH_FRACTION, 2);

  cache->reclaim.lowWater = max(nObj / RECLAIM_LOW_FRACTION, 1);
  cache->reclaim.hiWater = max(nObj / RECLAIM_HIGH_FRACTION, 2);
}

static inline void
cache_newobj_setup_aging(ObjectHeader *hdr)
{
  agelist_addFront(&Cache.obCache[hdr->ty]->reclaim, hdr);
}

static int
page_physaddr_cmp(const void *lhs, const void *rhs)
{
  const Page *lhp = *(const Page **)lhs;
  const Page *rhp = *(const Page **)rhs;

  if (lhp->pa > rhp->pa)
    return 1;
  if (lhp->pa < rhp->pa)
    return -1;
  return 0;
}

Page *
obhdr_findPageFrame(kpa_t pa)
{
  if (Cache.page_byPhysAddr_count == 0)
    return 0;

  int l = 0;
  int r = Cache.page_byPhysAddr_count - 1;

  while (r >= l) {
    int m = (l + r) / 2;
    Page *pm = Cache.page_byPhysAddr[m];
    if (pa == pm->pa)
      return pm;

    if (pa < pm->pa)
      r = m - 1;
    else
      l = m + 1;
  }
  return 0;
}

void
cache_add_page_space(bool lastCall)
{
  static size_t nPage = 0;
  size_t contigPages = 0;

  do {
    contigPages = 
      pmem_Available(&pmem_need_pages, COYOTOS_PAGE_SIZE, true);

    if (contigPages + nPage > Cache.c_Page.count)
      bug("Have processed %d pages. Now adding %d more (tot %d), but\n"
	  "  estimate only allowed for %d\n",
	  nPage, contigPages, 
	  nPage + contigPages, Cache.c_Page.count);
    else
      DEBUG_CACHE 
	printf("Have processed %d pages. Now adding %d more (tot %d)."
	       " Estimate\n allows for %d\n",
	    nPage, contigPages, 
	    nPage + contigPages, Cache.c_Page.count);

    if (contigPages) {
      kpa_t pa =
	pmem_AllocBytes(&pmem_need_pages, 
			contigPages * COYOTOS_PAGE_SIZE,
			pmu_PAGES, "page space");

      DEBUG_CACHE 
	printf("Added %d from pa=0x%016llx\n", contigPages, pa);

      assert(pa);

      /* We are allocating headers out of the header free list,
	 initializing the information about the associated frame, and
	 then adding the result into the obhash table and the aging
	 linked list. */
      for (size_t i = 0; i < contigPages; i++) {
	Page *phdr = cache_alloc_page_header();
	assert(phdr);

	phdr->pa = pa;
	phdr->mhdr.hdr.oid = 
	  coyotos_Range_physOidStart + (pa / COYOTOS_PAGE_SIZE);
	phdr->mhdr.hdr.ty = ot_Page;

	Cache.page_byPhysAddr[Cache.page_byPhysAddr_count++] = phdr;

	obhash_insert_obj(phdr);
	
	cache_newobj_setup_aging(&phdr->mhdr.hdr);

	pa += COYOTOS_PAGE_SIZE;
	nPage++;
      }
    }
  } while(contigPages);

  printf("Page space is %d pages (est. was %d)\n", nPage, Cache.c_Page.count);

  shellsort(Cache.page_byPhysAddr,
	    Cache.page_byPhysAddr_count,
	    sizeof (Cache.page_byPhysAddr),
	    page_physaddr_cmp);

  /* Update the watermarks using the new page count */
  cache_setup_watermarks(&Cache.c_Page, nPage);

  // This is a completely unmotivated guess.
  enum {
    NPAGE_PER_CAPPAGE = 10
  };
  Cache.max_oid[ot_Page] = nPage - (nPage / NPAGE_PER_CAPPAGE);
  Cache.max_oid[ot_CapPage] = (nPage / NPAGE_PER_CAPPAGE);

  DEBUG_CACHE 
    printf("Page space is %d pages (est. was %d)\n", nPage, Cache.c_Page.count);

  printf("nProc %d ngpt %d nendpt %d ndep %d nrm %d npage %d\n"
	 "  nOB %d nOTE %d\n", 
	 Cache.c_Process.count,
	 Cache.c_GPT.count,
	 Cache.c_Endpoint.count,
	 Cache.dep.count,
	 Cache.rmap.count,
	 Cache.c_Page.count,
	 Cache.c_Process.count + Cache.c_GPT.count + Cache.c_Endpoint.count +
	 Cache.c_Page.count,
	 Cache.ote.count);

  // Report out how much memory we have to work with
  DEBUG_CACHE 
    printf("Remaining allocatable bytes: 0x%016llx\n",
	   pmem_Available(&pmem_need_bytes, 1, false));

  // pmem_showall();
}

#define OTHER_CONSTRUCT(cname)						\
  do {									\
    Cache.cname.vec = calloc(sizeof(*Cache.cname.vec), Cache.cname.count); \
    freelist_init(&Cache.cname.freeList);				\
									\
    for (size_t i = 0; i < Cache.cname.count; i++) {			\
      freelist_insert(&Cache.cname.freeList, &Cache.cname.vec[i]);	\
    }									\
  } while(0);
    
#define OBCACHE_CONSTRUCT(cache, vector, oty)				\
  do {									\
    agelist_init(&Cache.cache.active);					\
    agelist_init(&Cache.cache.check);					\
    agelist_init(&Cache.cache.reclaim);					\
    Cache.vector = calloc(sizeof(*Cache.vector), Cache.cache.count);	\
    Cache.max_oid[oty] = Cache.cache.count;				\
  } while (0)

#define OBFRAME_CONSTRUCT(cache, vector, oty)				\
  OBCACHE_CONSTRUCT(cache, vector, oty);				\
  do {									\
    cache_setup_watermarks(&Cache.cache, 0);				\
    for (size_t i = 0; i < Cache.cache.count; i++) {			\
      Cache.vector[i].hdr.ty = oty;					\
      Cache.vector[i].hdr.oid = coyotos_Range_physOidStart + i;		\
      link_init(&Cache.vector[i].hdr.ageLink);				\
      obhash_insert_obj(&Cache.vector[i]);				\
      cache_newobj_setup_aging(&Cache.vector[i].hdr);			\
    }									\
  } while (0);

#define PROC_OBFRAME_CONSTRUCT(cache, vector, oty)			\
  OBCACHE_CONSTRUCT(cache, vector, oty);				\
  do {									\
    cache_setup_watermarks(&Cache.cache, 0);				\
    for (size_t i = 0; i < Cache.cache.count; i++) {			\
      Cache.vector[i].hdr.ty = oty;					\
      Cache.vector[i].hdr.oid = coyotos_Range_physOidStart + i;		\
      link_init(&Cache.vector[i].hdr.ageLink);				\
      link_init(&Cache.vector[i].queue_link);				\
      sq_Init(&Cache.vector[i].rcvWaitQ);				\
      obhash_insert_obj(&Cache.vector[i]);				\
      cache_newobj_setup_aging(&Cache.vector[i].hdr);			\
    }									\
  } while (0);

#define GPT_OBFRAME_CONSTRUCT(cache, vector, oty)			\
  OBCACHE_CONSTRUCT(cache, vector, oty);				\
  do {									\
    cache_setup_watermarks(&Cache.cache, 0);				\
    for (size_t i = 0; i < Cache.cache.count; i++) {			\
      Cache.vector[i].mhdr.hdr.ty = oty;				\
      Cache.vector[i].mhdr.hdr.oid = coyotos_Range_physOidStart + i;	\
      link_init(&Cache.vector[i].mhdr.hdr.ageLink);			\
      obhash_insert_obj(&Cache.vector[i]);				\
      cache_newobj_setup_aging(&Cache.vector[i].mhdr.hdr);		\
    }									\
  } while (0);

/* NOTE: Only putting frame header on page frame header free list,
   because we do not yet know which of these frames are backed. */
#define PAGE_FRAME_HEADER_CONSTRUCT(cache, vector, oty)	\
  Cache.page_byPhysAddr = calloc(sizeof(*Cache.page_byPhysAddr),\
				 Cache.cache.count);		\
  Cache.page_byPhysAddr_count = 0;				\
  OBCACHE_CONSTRUCT(cache, vector, oty);			\
  Cache.max_oid[oty] = 0;					\
  do {								\
    for (size_t i = 0; i < Cache.cache.count; i++) {		\
      Cache.vector[i].mhdr.hdr.ty = oty;			\
      link_init(&Cache.vector[i].mhdr.hdr.ageLink);		\
      link_insertAfter(&Cache.freePageHeaders,			\
		       &Cache.vector[i].mhdr.hdr.ageLink);	\
    }								\
  } while (0);


void
cache_init()
{
  link_init(&Cache.freePageHeaders);

#define DO_CONST(func, type)   func(c_ ## type, v_ ## type, ot_ ##type)

  DO_CONST(PROC_OBFRAME_CONSTRUCT,	Process);
  DO_CONST(GPT_OBFRAME_CONSTRUCT,	GPT);
  DO_CONST(OBFRAME_CONSTRUCT,		Endpoint);
  DO_CONST(PAGE_FRAME_HEADER_CONSTRUCT,	Page);

  OTHER_CONSTRUCT(dep);
  OTHER_CONSTRUCT(rmap);
  OTHER_CONSTRUCT(ote);

  cache_add_page_space(false);
}

/** @brief run the ager if necessary
 *
 * @invariant When this is called, we should already be holding the
 * lock on the object frame cache. This guards the age list.
 */
static void 
cache_age_framecache(ObFrameCache *ofc)
{
  if (agelist_underLowWater(&ofc->reclaim))
    ofc->reclaim.needsFill = true;
  if (agelist_underLowWater(&ofc->check))
    ofc->check.needsFill = true;

  while (ofc->check.needsFill | ofc->reclaim.needsFill) {
    if (ofc->reclaim.needsFill) {
      /*
       * Fill the Reclaim list to its high watermark.  To do this, we pull
       * the oldest object off the Check list, ask the HAL if the object
       * was referenced in a way which wouldn't cause cache_upgrade_age()
       * to be called.  If it had been, we move it to the active list.  
       * Otherwise, we invalidate all outstanding capabilities to the object
       * and any cached state, and put it on the reclaim list.
       */
      while (agelist_underHiWater(&ofc->reclaim) &&
	     !agelist_isEmpty(&ofc->check)) {
	ObjectHeader *hdr = agelist_oldest(&ofc->check);
	/* An object in active use by this transaction cannot be on the
	 * "check in use" list.
	 */
	assert(!mutex_isheld(&hdr->lock));
	HoldInfo hi = mutex_grab(&hdr->lock);
	/* The HAT may also discover that an object is actually in use. */
	bool was_refed = object_was_referenced(hdr);

	agelist_removeOldest(&ofc->check, hdr);

	OTEntry *ote = atomic_read_ptr(&hdr->otIndex);
	assert(OTINDEX_IS_CHECKREF(ote));
	
	if (was_refed) {
	  agelist_addFront(&ofc->active, hdr);
	  atomic_write_ptr(&hdr->otIndex, OTINDEX_UNCHECKREF(ote));
	} else {
	  agelist_addFront(&ofc->reclaim, hdr);
	  obhdr_invalidate(hdr);
	}
	mutex_release(hi);
      }
      /* 
       * If we didn't get to the high water mark, loop around again after
       * we fill the check list.  But if we're out of active objects, there's
       * no way to generate reclaimable objects.
       */
      if (!agelist_underHiWater(&ofc->reclaim) &&
	  !agelist_isEmpty(&ofc->active))
	ofc->reclaim.needsFill = false;
      if (agelist_underLowWater(&ofc->check))
	ofc->check.needsFill = true;
    }
    if (ofc->check.needsFill) {
      // if we're not trying to fill the reclaim cache, wait until there
      // are enough objects in the "active" set before dumping them in
      // check
      if (!ofc->reclaim.needsFill && agelist_underLowWater(&ofc->active)) {
	ofc->check.needsFill = 0;
	break;
      }

      /*
       * Fill the Check list to the high water mark.  Pull objects off
       * of the Active list, set their otIndex to CHECKREF, and put
       * the objects at the front of the Check list.  The flagged
       * otIndex will cause cap_prepare()'s otIndex check to fail,
       * which will induce a call to cache_upgrade_age(), which will
       * move the frame back to the active list with the CHECKREF bit
       * cleared.
       */
      size_t nInTrans = 0;
      while (agelist_underHiWater(&ofc->check) &&
	     agelist_hasMoreThan(&ofc->active, nInTrans)) {
	ObjectHeader *hdr = agelist_oldest(&ofc->active);
	/* If we're holding the object for this operation, or it is pinned,
	 * it is in active use.  Move it to the front of the active list.
	 */
	if (hdr->pinned || mutex_isheld(&hdr->lock)) {
	  nInTrans++;
	  agelist_removeOldest(&ofc->active, hdr);
	  agelist_addFront(&ofc->active, hdr);
	  continue;
	}
	HoldInfo hi = mutex_grab(&hdr->lock);
	object_begin_refcheck(hdr);
	agelist_removeOldest(&ofc->active, hdr);

	OTEntry *ote = atomic_read_ptr(&hdr->otIndex);
	assert(ote != OTINDEX_INVALID && !OTINDEX_IS_CHECKREF(ote));
	atomic_write_ptr(&hdr->otIndex, OTINDEX_CHECKREF(ote));
	agelist_addFront(&ofc->check, hdr);
	mutex_release(hi);
      }
      /* 
       * If we run out of active objects, there's no way to generate more,
       * so we always stop working.
       */
      ofc->check.needsFill = false;
    }
  }
}

/** @brief generic allocator from an object frame cache. */
static ObjectHeader *
obframecache_alloc(ObFrameCache *ofc)
{
  HoldInfo hi = mutex_grab(&ofc->lock);

  cache_age_framecache(ofc);

  assert(!link_isSingleton(&ofc->reclaim.list));
  ObjectHeader *ob = agelist_oldest(&ofc->reclaim);

  /* An object in active use by this transaction cannot be on the
   * reclaim list.
   */
  assert(!mutex_isheld(&ob->lock));

  /* 
   * the object must have no outstanding references. 
   */
  assert(atomic_read_ptr(&ob->otIndex) == OTINDEX_INVALID);

  agelist_removeOldest(&ofc->reclaim, ob);

  obhash_remove_obj(ob);			
  mutex_release(hi);					
  
  ob->current = 0;
  ob->snapshot = 0;
  cache_clear_object(ob);
  
  return ob;
}

// Need to deal with ObStore locking issues here.
#define OB_ALLOC(type)					  \
  do {							  \
    return (type *)obframecache_alloc(&Cache.c_ ## type); \
  } while (0)

ObjectHeader *
cache_alloc(ObType oty)
{
  return obframecache_alloc(Cache.obCache[oty]);
}

Process *
cache_alloc_process(void)
{
  OB_ALLOC(Process);
}

GPT *
cache_alloc_GPT(void)
{
  OB_ALLOC(GPT);
}

Page *
cache_alloc_page(void)
{
  OB_ALLOC(Page);
}

Endpoint *
cache_alloc_endpoint(void)
{
  OB_ALLOC(Endpoint);
}

void
cache_upgrade_age(ObjectHeader *hdr, OTEntry *newidx)
{
  ObFrameCache *ofc = Cache.obCache[hdr->ty];

  HoldInfo hi = mutex_grab(&ofc->lock);
  OTEntry *p = atomic_read_ptr(&hdr->otIndex);
  bool run_ager = false;

  if (p == NULL) {
    /*
     * object is on the reclaim list.  Take him off, and install his OTIndex.
     */
    link_unlink(&hdr->ageLink);
    assert(ofc->reclaim.count > 0);
    ofc->reclaim.count--;

    run_ager = true;
  } else {
    /* the otIndex must be marked as in "check" list, and newidx must match */
    assert(OTINDEX_IS_CHECKREF(p) && OTINDEX_UNCHECKREF(p) == newidx);

    link_unlink(&hdr->ageLink);
    assert(ofc->check.count > 0);
    ofc->check.count--;
  }

  atomic_write_ptr(&hdr->otIndex, newidx);

  agelist_addFront(&ofc->active, hdr);

  if (run_ager)
    cache_age_framecache(ofc);

  mutex_release(hi);
}

void
cache_install_new_object(ObjectHeader *hdr)
{
  ObFrameCache *ofc = Cache.obCache[hdr->ty];
  HoldInfo hi = mutex_grab(&ofc->lock);
  OTEntry *idx = cache_alloc_OTEntry();
  idx->oid = hdr->oid;

  assert(atomic_read_ptr(&hdr->otIndex) == OTINDEX_INVALID);
  atomic_write_ptr(&hdr->otIndex, idx);
  agelist_addFront(&ofc->active, hdr);
  mutex_release(hi);
}

#define OTHER_ALLOC(cname, type)				\
  do {								\
    type *ret = (type *) freelist_alloc(&Cache.cname.freeList);	\
      INIT_TO_ZERO(ret);					\
    return ret;							\
  } while (0)


OTEntry *
cache_alloc_OTEntry(void)
{
  /// @bug Need to drive some GC from here!!!
  OTEntry *ote = (OTEntry *) freelist_alloc(&Cache.ote.freeList);
  assert(ote != 0);

  // Need to set the mark bit in case a GC is in progress and the OTE
  // mark pass is over. Currently allocated OTEs will survive current
  // GC pass if this is the case.
  ote->oid = 0;
  atomic_write(&ote->flags, OTE_MARK);

  return ote;
}

Depend *
cache_alloc_Depend(void)
{
  OTHER_ALLOC(dep, Depend);
}

RevMap *
cache_alloc_RevMap(void)
{
  OTHER_ALLOC(rmap, RevMap);
}

Page *
cache_alloc_page_header(void)
{
  HoldInfo hi = mutex_grab(&Cache.freePageHeadersLock);

  assert(!link_isSingleton(&Cache.freePageHeaders));
  Page *pg = (Page *) Cache.freePageHeaders.next;

  link_unlink(Cache.freePageHeaders.next);

  mutex_release(hi);

  return pg;
}

void
cache_clear_object(ObjectHeader *ob)
{
  assert(ob->dirty || !(ob->current || ob->snapshot));

  switch(ob->ty) {
  case ot_Page:
  case ot_CapPage:
    {
      Page *pg = (Page *)ob;
      memset_p(pg->pa, 0, COYOTOS_PAGE_SIZE);
      break;
    }
  case ot_Process:
    {
      /** @bug Process must not be running or receiving! */
      /** @bug Need to hold the per-process receive wait lock */
      Process *p = (Process *)ob;
      atomic_write(&p->issues, 0);
      p->lastCPU = 0;		/* doesn't really matter */
      p->readyQ = 0;
      assert(p->onQ == 0);
      assert(link_isSingleton(&p->queue_link));
      p->mappingTableHdr = 0;
      assert(sq_IsEmpty(&p->rcvWaitQ));
      assert(p->ipcPeer == 0);

      /** @bug Do we need to keep any flags bits? Execution model bit? */
      memset(&p->state, 0, sizeof(p->state));
      p->state.runState = PRS_FAULTED;
      break;
    }
  case ot_Endpoint:
    {
      Endpoint *ep = (Endpoint *)ob;
      memset(&ep->state, 0, sizeof(ep->state));
      break;
    }
  case ot_GPT:
    {
      GPT *gpt = (GPT *)ob;
      memset(&gpt->state, 0, sizeof(gpt->state));
      break;
    }
  }

  /// @bug Should recompute checksum here.
  ob->cksum = 0;
  return;
}

void
cache_write_back_object(ObjectHeader *ob)
{
  if (ob->dirty) {
    assert(false && "write me back!");
  }
}

void 
endpt_gc(Endpoint *endpt)
{
  cap_gc(&endpt->state.recipient);
}

void 
gpt_gc(GPT *gpt)
{
  for (size_t i = 0; i < NUM_GPT_SLOTS; i++)
    cap_gc(&gpt->state.cap[i]);
}

void
page_gc(Page *p)
{
  if (p->mhdr.hdr.ty == ot_CapPage) {
    capability *arr = TRANSMAP_MAP(p->pa, capability *);
    for (size_t i = 0; i < COYOTOS_PAGE_SIZE / sizeof (capability); i++)
      cap_gc(&arr[i]);
    TRANSMAP_UNMAP(arr);
  }
}

void 
proc_gc(Process *p)
{
  cap_gc(&p->state.schedule);
  cap_gc(&p->state.addrSpace);
  cap_gc(&p->state.brand);
  cap_gc(&p->state.cohort);
  cap_gc(&p->state.handler);
  for (size_t i = 0; i < NUM_CAP_REGS; i++)
    cap_gc(&p->state.capReg[i]);
}

Page *
cache_get_physPage(kpa_t pa)
{
  oid_t oid = coyotos_Range_physOidStart + (pa / COYOTOS_PAGE_SIZE);  

  /** @bug when we have physical ranges coming and going, this will
   * need to do some locking
   */
  Page *page = obhdr_findPageFrame(pa);
  if (page == 0)
    return 0;

  HoldInfo hi = mutex_grab(&page->mhdr.hdr.lock);
  if (page->mhdr.hdr.oid == oid) {
    return page;
  }

  if (page->mhdr.hdr.pinned) {
    // cannot allocate pinned object
    mutex_release(hi);
    return 0;
  }

  // we need to move aside the existing data, and invalidate any cached
  // mappings
  HoldInfo ob1_hi = obhash_grabMutex(ot_Page, oid);
  HoldInfo ob2_hi = obhash_grabMutex(page->mhdr.hdr.ty,
				     page->mhdr.hdr.oid);

  Page *npage = cache_alloc_page();
  
  if (page != npage) {
    /* now that we're holding the bucket lock, invalidate the existing page. */
    memhdr_invalidate_cached_state(&page->mhdr);
    obhdr_invalidate(&page->mhdr.hdr);

    /* copy the data */
    memcpy_ptop(npage->pa, page->pa, COYOTOS_PAGE_SIZE);

    obhash_remove_obj(page);
    npage->mhdr.hdr.ty = page->mhdr.hdr.ty;
    npage->mhdr.hdr.oid = page->mhdr.hdr.oid;
    npage->mhdr.hdr.allocCount = page->mhdr.hdr.allocCount;
    npage->mhdr.hdr.hasDiskCaps = page->mhdr.hdr.hasDiskCaps;
    npage->mhdr.hdr.current = page->mhdr.hdr.current;
    npage->mhdr.hdr.snapshot = page->mhdr.hdr.snapshot;
    npage->mhdr.hdr.dirty = page->mhdr.hdr.dirty;
    npage->mhdr.hdr.pinned = 0;
    npage->mhdr.hdr.cksum = page->mhdr.hdr.cksum;

    obhash_insert_obj(npage);
    cache_install_new_object(&npage->mhdr.hdr);  
  }
  mutex_release(ob2_hi);

  memset_p(page->pa, 0, COYOTOS_PAGE_SIZE);

  page->mhdr.hdr.ty = ot_Page;
  page->mhdr.hdr.oid = oid;
  page->mhdr.hdr.allocCount = 0;
  page->mhdr.hdr.hasDiskCaps = 0;
  page->mhdr.hdr.current = 1;
  page->mhdr.hdr.snapshot = 0;
  page->mhdr.hdr.dirty = 1;
  page->mhdr.hdr.pinned = 1;

  obhash_insert_obj(page);

  mutex_release(ob1_hi);

  return page;
}

void
cache_preload_image(const char *name, kpa_t base, size_t size)
{
  static int hasrun = 0;

  /** @bug It may turn out that this should not be fatal... */
  if (hasrun)
    fatal("cache_preload_image() called multiple times\n");
  hasrun = 1;

  // load a image file output by mkimage into the cache
  if (((uintptr_t)base & COYOTOS_PAGE_ADDR_MASK) != 0)
    fatal("%s: not page-aligned (%p)\n", name, base);
  if (size < COYOTOS_PAGE_SIZE)
    fatal("%s: too short (0x%lx)\n", name, size);

  CoyImgHdr hdr;
  memcpy_ptov(&hdr, base, sizeof (hdr));

  printf("cache_preload_image(%llx, %lx)\n", base, size);
  if (memcmp(hdr.magic, "coyimage", 8) != 0) {
    char out[9];
    memcpy(out, hdr.magic, 8);
    out[8] = 0;
    fatal("%s: bad magic string: %s\n", name, out);
  }

  if (hdr.endian != BYTE_ORDER)
    fatal("%s: bad byte order (%d, expected %d)\n", name, 
	  hdr.endian, BYTE_ORDER);

  if (hdr.version != 1)
    fatal("%s: bad version (%d, expected %d)\n", name, 
	  hdr.version, 1);

  if (hdr.target != COYOTOS_TARGET)
    fatal("%s: bad target arch (%d, expected %d)", name,
	  hdr.target, COYOTOS_TARGET);

  if (hdr.pgSize != COYOTOS_PAGE_SIZE)
    fatal("%s: bad page size (0x%lx, expected 0x%lx)\n", name,
	  hdr.pgSize, COYOTOS_PAGE_SIZE);

  if (hdr.imgBytes != size)
    fatal("%s: imgBytes (0x%lx) != size (0x%lx)\n",
	  name, hdr.imgBytes, size);

  /* Sanity check that we don't have a structure size mismatch */
  uint32_t expectedBytes = 
    hdr.nPage * COYOTOS_PAGE_SIZE +
    hdr.nCapPage * COYOTOS_PAGE_SIZE +
    hdr.nGPT * sizeof(ExGPT) +
    hdr.nEndpoint * sizeof(ExEndpoint) +
    hdr.nProc * OBSTORE_EXPROCESS_COMMON_SIZE;

  if (expectedBytes != hdr.imgBytes)
    fatal("%s: Size mismatch in image structures.\n", name);

  uintptr_t cur = base;

  size_t idx;
  for (idx = 0; idx < hdr.nPage; idx++) {
    Page *pg = cache_alloc_page();

    pg->mhdr.hdr.ty = ot_Page;
    pg->mhdr.hdr.oid = idx;
    pg->mhdr.hdr.allocCount = 0;
    pg->mhdr.hdr.current = 1;

    // At the moment, we don't try to elide zero pages.  If we did,
    // it would effect this code.
    memcpy_ptop(pg->pa, cur, COYOTOS_PAGE_SIZE);
    cur += COYOTOS_PAGE_SIZE;

    cache_install_new_object(&pg->mhdr.hdr);
    obhash_insert_obj(pg);
  }

  for (idx = 0; idx < hdr.nCapPage; idx++) {
    Page *pg = cache_alloc_page();

    pg->mhdr.hdr.ty = ot_CapPage;
    pg->mhdr.hdr.oid = idx;
    pg->mhdr.hdr.allocCount = 0;
    pg->mhdr.hdr.current = 1;

    // validate capabilities ?
    memcpy_ptop(pg->pa, cur, COYOTOS_PAGE_SIZE);
    cur += COYOTOS_PAGE_SIZE;

    cache_install_new_object(&pg->mhdr.hdr);
    obhash_insert_obj(pg);
  }

  for (idx = 0; idx < hdr.nGPT; idx++) {
    GPT *gpt = cache_alloc_GPT();

    gpt->mhdr.hdr.ty = ot_GPT;
    gpt->mhdr.hdr.oid = idx;
    gpt->mhdr.hdr.allocCount = 0;
    gpt->mhdr.hdr.current = 1;

    memcpy_ptov(&gpt->state, cur, sizeof (gpt->state));
    // validate capabilities ?
    cur += sizeof (gpt->state);

    cache_install_new_object(&gpt->mhdr.hdr);
    obhash_insert_obj(gpt);
  }

  for (idx = 0; idx < hdr.nEndpoint; idx++) {
    Endpoint *ep = cache_alloc_endpoint();

    ep->hdr.ty = ot_Endpoint;
    ep->hdr.oid = idx;
    ep->hdr.allocCount = 0;
    ep->hdr.current = 1;

    memcpy_ptov(&ep->state, cur, sizeof (ep->state));
    // validate capabilities ?
    cur += sizeof (ep->state);
    
    cache_install_new_object(&ep->hdr);
    obhash_insert_obj(ep);
  }

  for (idx = 0; idx < hdr.nProc; idx++) {
    Process *proc = cache_alloc_process();

    proc->hdr.ty = ot_Process;
    proc->hdr.oid = idx;
    proc->hdr.allocCount = 0;
    proc->hdr.current = 1;
    atomic_write(&proc->issues, pi_IssuesOnLoad);

    proc->mappingTableHdr = 0;

    // The data structure coming from mkimage omits a bunch of the
    // process state. Zero the whole thing before overwriting the
    // leading subset.
    INIT_TO_ZERO(&proc->state);
    memcpy_ptov(&proc->state, cur, OBSTORE_EXPROCESS_COMMON_SIZE);

    // validate capabilities ?
    cur += OBSTORE_EXPROCESS_COMMON_SIZE;
    
    cache_install_new_object(&proc->hdr);
    obhash_insert_obj(proc);

    // Add it to the end of the ready queue, but only if it is marked
    // as having a startup fault.
    if (proc->state.faultCode == coyotos_Process_FC_Startup) {
      proc->state.runState = PRS_RUNNING;
      atomic_write(&proc->issues, pi_Faulted);
      rq_add(&mainRQ, proc, false);
    }
  }

  if (cur - base > size)
    fatal("%s: ran over the size\n", name);

  printf("%s: loaded "
	 " nPage=%d nCapPage=%d nGPT=%d nEndpoint=%d nProc=%d\n", name,
	 hdr.nPage, hdr.nCapPage, hdr.nGPT, hdr.nEndpoint, hdr.nProc);
}

