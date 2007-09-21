#ifndef __KERNINC_OBJECTCACHE_H__
#define __KERNINC_OBJECTCACHE_H__
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
 * @brief Interface to the kernel object cache. */

#include <stdbool.h>
#include <kerninc/capability.h>
#include <kerninc/mutex.h>
#include <kerninc/AgeList.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/FreeList.h>
#include <kerninc/RevMap.h>

/* Since all of the various object caches are globals, we can use a
   macro to simplify declaring them in a type-safe way.

   Object frame caches differ in that they don't really have a
   conventional free list. We eliminated that by performing a
   canonical initialization to known-safe values and putting them
   directly into the object hash table. */

typedef struct ObFrameCache {
  /** @brief Protects the contents of this cache, and the @p ageLink fields
   * of every object in this cache.
   */
  mutex_t   	lock;
  /** @brief Number of objects in the associated vector. */
  size_t    	count;

  /** @brief Aged list of frames in active use */
  AgeList	active;
  /** @brief Aged list of frames which we are checking for active use. */
  AgeList	check;
  /** @brief Aged list of frames available for reclaiming */
  AgeList       reclaim;
} ObFrameCache;

/* Convention: non-object structures use a different free list
   convention. When the object is on the free list, its first word is
   re-used for the next pointer. This word is zeroed on
   allocation. Remember that 'word' is architecture-dependent. On some
   architectures this means the first 64-bits must not have any
   meaning if the object is free. */
#define CACHE(T)		\
  struct {			\
    mutex_t   lock;		\
    size_t    count;		\
    FreeListHeader freeList;	\
    struct T  *vec;		\
  }

typedef struct Cache_s {
  ObFrameCache * const obCache[ot_NTYPE];
  /** @brief Maximum oid allowed for each object type (actually, 1 greater)
   *
   * @bug This is a hack;  really, the Range/ObStore protocol should
   * set up valid OID ranges.
   */
  oid_t		max_oid[ot_NTYPE];


  mutex_t                   freePageHeadersLock;
  Link                      freePageHeaders;

  Page		**page_byPhysAddr;     /* array of Page pointers in PA order */
  size_t	page_byPhysAddr_count; /* # of elements in page_byPhysAddr */

/*
 * The vectors for each frame type are defined seperately from the
 * Cache structures, in order to simplify the structure definitions.
 */
#define DEFFRAME(ft, val) struct ft *v_ ## ft;
#define ALIASFRAME(alias_ft, ft, val)
#define NODEFFRAME(ft, val)
#include <kerninc/frametype.def>
#define DEFFRAME(ft, val) ObFrameCache c_ ## ft;
#define ALIASFRAME(alias_ft, ft, val)
#define NODEFFRAME(ft, val)
#include <kerninc/frametype.def>

  CACHE(Depend)             dep;
  CACHE(RevMap)             rmap;
  CACHE(OTEntry)            ote;
} Cache_s;


extern Cache_s Cache;

enum { NUM_OBHASH_BUCKETS = 1024 };
ObjectHeader *obhash[NUM_OBHASH_BUCKETS];

/** @brief Estimate the sizes on the various kernel object caches for
 * all caches that have not already been sized by the machine
 * dependent initialization code, returning results by side-effecting
 * the global Cache structure.
 *
 * This procedure is called from low-level initialization code before
 * the heap is initialized. This is necessary so that the machine
 * dependent code can use the results to allocate any associated
 * machine dependent data structures.  It is safe to call the pmem_*
 * family of functions and printf() here, but not much else.
 *
 * Note that the nPage value is not definitive until page space is
 * actually allocated, because this estimate does not take into
 * account any physical frames that will be allocated for machine
 * dependent purposes such as mapping structures. The goal in
 * cache_estimate_sizes() is to @em overestimate this requirement,
 * because we need to ensure that there will be enough object headers
 * for all of the pages we actually end up with.
 *
 * @bug The size of the Cache.dep structure is extremely architecture
 * sensitive, and should almost certainly be set by machine dependent
 * code.
 */
extern void cache_estimate_sizes(size_t pagesPerProc, kpsize_t totPage);

/** Donate physical pages to the page cache.
 *
 * This routine may be called twice: the first time to allocate
 * initial page space and again after the BSP package copies the boot
 * image into page space and reviews any protected/reserved memory
 * locations.
 */
extern void cache_add_page_space(bool lastCall);

/** @brief Initialize the kernel object caches.
 *
 * We need to allocate permanent storage for several object
 * ``caches'' and initialize them. Many of the things allocated here
 * are machine-neutral:
 * <ul>
 * <li>The Process cache
 * <li>The GPT cache
 * <li>The Endpoint cache
 * <li>The Page cache (headers are neutral)
 * </ul>
 * And some are machine-dependent:
 * <ul>
 * <li> The Depend cache
 * <li> Page directory space
 * <li> Page table space
 * </ul>
 *
 * For non-page objects, we initialize the actual object frames. For
 * page frame headers, we initialize the header onto a free list of
 * page frame headers, but we cannot initialize the per-frame state
 * until we know what actual page frames will exist.
 */
extern void cache_init(void);

/**
 * @brief Preload a mkimage-generated image at physaddr @p base with
 * size @p size.  If image loading fails, we will panic.
 */
extern void cache_preload_image(const char *name, kpa_t base, size_t size);

/** @brief Allocate a Page Header Frame */
extern struct Page *cache_alloc_page_header(void);
/** @brief Allocate a Page Frame */
extern struct Page *cache_alloc_page(void);
/** @brief Allocate a GPT Frame */
extern struct GPT *cache_alloc_GPT(void);
/** @brief Allocate an Endpoint */
extern struct Endpoint *cache_alloc_endpoint(void);
/** @brief Allocate a Process */
extern struct Process *cache_alloc_process(void);
/** @brief Allocate an OT Entry */
extern struct OTEntry *cache_alloc_OTEntry(void);
/** @brief Allocate a Depend structure */
extern struct Depend *cache_alloc_Depend(void);
/** @brief Allocate a RevMap structure */
extern struct RevMap *cache_alloc_RevMap(void);

extern struct ObjectHeader *cache_alloc(ObType ty);

/** @brief Installs a new object into the aging system. */
extern void cache_install_new_object(ObjectHeader *hdr);

/** @brief Write object to backing store if required. * from memory. 
 *
 * When called, object is no longer in object hash table (which may
 * not be right). 
 *
 * @bug What this actually does is wipe the object state, which should
 * be handled separately.
 *
 * @precondition Object must already be invalidated.
 */
extern void cache_write_back_object(ObjectHeader *ob);

/** @brief Clear the object to a ``zero'' state without altering
 * object identity.
 *
 * @precondition Object must already be invalidated.
 */
extern void cache_clear_object(ObjectHeader *ob);

/** @brief upgrade the age of an object.
 *
 * The object must either be: 
 * 
 *   @li on the check list (i.e. its otIndex is CHECKREF), in which case 
 *       @p newidx should be the otIndex without the CHECKREF bit set, or 
 *   @li on the reclaim list (i.e. its otIndex is INVALID), in which case
 *       @p newidx should be a new OTEntry with the OID field already set up.
 */
void cache_upgrade_age(ObjectHeader *hdr, OTEntry *newidx);

/** @brief gets a physical page key with physical address @p pa.
 *
 * The Page will be locked in memory until cache_release_physPage() is called.
 */
struct Page *cache_get_physPage(kpa_t pa);

/** @brief release the pin on a physical page key. */
void cache_release_physPage(struct Page *page);

#endif /* __KERNINC_OBJECTCACHE_H__ */
