/*
 * Copyright (C) 2005, Jonathan S. Shapiro.
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
 * @brief Kernel heap management.
 *
 * The kernel heap is used for all data structures that require
 * virtually contiguous mappings. Allocation from the heap does
 * <em>not</em> guarantee physically contiguous mappings. If you need
 * that, you need to talk to the physical memory allocation logic.
 * The Coyotos kernel makes much heavier use of the heap than the EROS
 * kernel did, though it still does so primarily at initialization
 * time. Once the system is initialized, the heap will be used only
 * when device drivers are advising the kernel of new device memory
 * regions. Even in that case, the heap is used only for creation of
 * the per-page frame structures for that memory.
 *
 * Bootstrapping the kernel heap is a little delicate. In abstract, we
 * would prefer to allocate frames for the heap from page
 * space. Unfortunately, we would also like to use the heap fairly
 * early during the bootstrap code before page space has been
 * allocated.
 *
 * In general, the rule is: <b>a heap frame is never returned</b>. To
 * ensure that this rule can be satisfied, we never allocate heap
 * frames from dismountable memory.
 *
 * @todo If we ever need to support machine(s) with hot-plug memory
 * (e.g. Tandem) we will need a mechanism to do an exchange of frames
 * before removing the memory from the bus. In abstract, this is not
 * difficult, but I haven't implemented it.
 *
 */

#include <stddef.h>
#include <hal/kerntypes.h>
#include <kerninc/assert.h>
#include <kerninc/util.h>
#include <kerninc/printf.h>
#include <kerninc/string.h>
#include <kerninc/util.h>
#include <kerninc/PhysMem.h>
#include <kerninc/Cache.h>
#include <kerninc/mutex.h>
#include <hal/vm.h>

extern void _end();

#define DEBUG_HEAP if (0)

/* Local Variables: */
/* comment-column:30 */
/* End: */

static kva_t heap_start = (kva_t) &_end; /**<! @brief starting VA of heap */
static kva_t heap_end = (kva_t) &_end; /**<! @brief current end of heap */

/**<! @brief Heap is backed to here. */
static kva_t heap_backed = (kva_t) &_end;

/** @brief Hard upper bound for heap lest we collide.
 *
 * If this is non-zero, heap has been initialized.
 */
static kva_t heap_hard_limit = 0;

void
heap_init(kva_t start, kva_t backedTo, kva_t limit)
{
  /* Initialization of heap_start, heap_backed, heap_end is handled
     in the machine-specific MapKernel code, which is called very
     early in mach_BootInit(). */

  heap_start = start;
  heap_end = start;
  heap_backed = align_up(backedTo, COYOTOS_PAGE_SIZE);
  heap_hard_limit = limit;

  DEBUG_HEAP 
    printf("heap_start, heap_end = 0x%08x, 0x%08x\n", heap_start, heap_end);
}

/* static */ void
grow_heap(kva_t target)
{
  assert(heap_hard_limit);
  assert((heap_backed % COYOTOS_PAGE_SIZE == 0));

  kva_t goal = align_up(target, COYOTOS_PAGE_SIZE);

  assert(goal <= heap_hard_limit);

  for(kva_t canmap = heap_backed; canmap < goal; canmap += COYOTOS_PAGE_SIZE)
    kmap_EnsureCanMap(canmap, "heap map");

  /* We are going to try to grow the heap by grabbing maximally large
     chunks of contiguous pages in order to avoid excessive
     fragmentation at the physical memory layer. If we cannot get
     pages from the pmem layer, we will attempt to get them from page
     space. */

  DEBUG_HEAP 
    printf("heap_backed: 0x%08x target: 0x%08x goal: 0x%08x\n", 
	   heap_backed, target, goal);

  while (heap_backed < goal) {
    size_t contigPages = 
      pmem_Available(&pmem_need_pages, COYOTOS_PAGE_SIZE, true);
    
    DEBUG_HEAP 
      printf("  heap_backed: 0x%08x target: 0x%08x goal: 0x%08x\n", 
	     heap_backed, target, goal);

    if (contigPages) {
      size_t nPage = 
	min((goal - heap_backed) / COYOTOS_PAGE_SIZE, contigPages);

      kpa_t pa = 
	pmem_AllocBytes(&pmem_need_pages, 
			nPage * COYOTOS_PAGE_SIZE,
			pmu_KHEAP, "heap frames");

      // FIX: pmem_AllocBytes could fail.

      DEBUG_HEAP 
	printf("heap: allocated %d pages at 0x%016x for kernel heap\n", 
	       nPage, pa);

      for (kpa_t base = pa; base < pa + (nPage * COYOTOS_PAGE_SIZE); 
	   base += COYOTOS_PAGE_SIZE, heap_backed += COYOTOS_PAGE_SIZE) {
	kmap_map(heap_backed, base, KMAP_R|KMAP_W);
	memset((void *) heap_backed, 0, COYOTOS_PAGE_SIZE);
      }
    }
    else
      // Fix this later to grab from object space.
      assert (0 && "TODO: Implement grabbing frames from page space\n");
  }

  DEBUG_HEAP
    printf("Heap now backed to 0x%08x\n", heap_backed);
}

#if 0
kpa_t
acquire_heap_page()
{
  if (physMem_ChooseRegion(EROS_PAGE_SIZE, &physMem_pages)) {
    return physMem_Alloc(EROS_PAGE_SIZE, &physMem_pages);
  }
  else {
    ObjectHeader *pHdr = 0;
    kpa_t pa;

    printf("Trying to allocate from object heap.\n");


    pHdr = objC_GrabPageFrame();

    pa = VTOP(objC_ObHdrToPage(pHdr));
    pHdr->obType = ot_PtKernelHeap;
    objH_SetFlags(pHdr, OFLG_DIRTY);	/* always */

    return pa;
  }
}
#endif

static mutex_t heap_mutex;

/**
 * @bug This implementation of malloc() is a placeholder, since free()
 * clearly would not work correcly without maintaining some sort of
 * record of what was allocated where.
 */
static void *
malloc(size_t nBytes)
{
  HoldInfo hi = mutex_grab(&heap_mutex);

  assert(heap_hard_limit);

  DEBUG_HEAP
    printf("malloc: heap start=0x%08x end=0x%08x backed=0x%08x limit=0x%08x alloc=%d\n",
	   heap_start, heap_end, heap_backed, heap_hard_limit, nBytes);

  // The address we return must be aligned at a pointer boundary. On
  // all currently forseen implementations this is the worst alignment
  // that the kernel must deal with.
  kva_t va = align_up(heap_end, sizeof(kva_t));

  // Extend the heap far enough to encompass the required number of
  // bytes:
  grow_heap(va + nBytes);

  heap_end = va + nBytes;

  DEBUG_HEAP
    printf("malloc: allocated %d bytes at 0x%08x\n", nBytes, va);

  mutex_release(hi);

  return (void *) va;
}

void *
calloc(size_t nBytes, size_t nElem)
{
  void *vp = malloc(nBytes * nElem);
  memset(vp, 0, nBytes * nElem);
  return vp;
}

void *
construct(size_t nBytes, size_t nElem, void (*initproc)(void *))
{
  void *vp = calloc(nBytes, nElem);

  unsigned char *ucp = vp;

  for (size_t i = 0; i < nElem; i++)
    if (initproc) initproc(ucp + (i * nBytes));

  return vp;
}
