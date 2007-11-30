/*
 * Copyright (C) 2005, The EROS Group, LLC.
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
 * @brief Low level, architecture-specific initialization.
 */

#include <stdbool.h>

#include <hal/kerntypes.h>
#include <hal/console.h>
#include <hal/machine.h>
#include <hal/config.h>
#include <hal/vm.h>
#include <hal/transmap.h>
#include <kerninc/assert.h>
#include <kerninc/ctype.h>
#include <kerninc/printf.h>
#include <kerninc/PhysMem.h>
#include <kerninc/string.h>
#include <kerninc/pstring.h>
#include <kerninc/ccs.h>
#include <kerninc/malloc.h>
#include <kerninc/util.h>
#include <kerninc/Cache.h>
#include <kerninc/CPU.h>
#include <kerninc/CommandLine.h>
#include <kerninc/util.h>

#include "IA32/CR.h"

#include "MultiBoot.h"
#include "kva.h"
#include "GDT.h"
#include "TSS.h"
#include "IRQ.h"
#include "PIC.h"
// #include "acpi.h"
#include "cpu.h"
#include "hwmap.h"

/// @brief Recorded multiboot signature.
uint32_t multibootSignature;
/// @brief Recorded multiboot information pointer.
uint32_t multibootInfo;

extern uint32_t _start[];
extern uint32_t _etext[];
extern uint32_t __syscallpg[];
extern uint32_t __esyscallpg[];
extern uint32_t _pagedata[];
extern uint32_t _end[];
extern uint32_t _bss_end[];
extern uint32_t __begin_maps[];
extern uint32_t __end_maps[];
extern uint32_t cpu0_kstack_lo[];
extern uint32_t cpu0_kstack_hi[];

extern void hardclock_init();

static PmemClass 
mmap_to_class(uint32_t multibootType)
{
  switch(multibootType) {
  case 1: return pmc_RAM;
  case 2: return pmc_ROM;
  case 3: return pmc_NVRAM;
  case 4: return pmc_RAM;
  default: assert(false);
  }
}

static PmemUse 
mmap_to_use(uint32_t multibootType)
{
  switch(multibootType) {
  case 1: return pmu_AVAIL;
  case 2: return pmu_BIOS;
  case 3: return pmu_ACPI_NVS;
  case 4: return pmu_ACPI_DATA;
  default: assert(false);
  }
}

#define MAX_MODULES 4
struct MultibootModuleInfo ModInfo[4];

/** @brief Initialize physical memory.
 *
 * This routine is <em>extremely</em> multiboot-specific.
 *
 * @todo This routine is *supposed* to get the physical memory layout,
 * but I have just determined that grub under QEMU isn't providing all
 * of the information that I want to gather, so I need to check what
 * it does on real hardware. The issue is that I want the ROM
 * locations as well. Drat!
 * 
 * @todo The case !MBI_MMAP and MBI_VALID should only appear on
 * ancient legacy BIOS's that are no longer shipping. It is not clear
 * whether we should still be supporting this.
 */
static void
config_physical_memory(void)
{
  /* On entry, only the low 1G (0x40000000) is mapped! */

  if (multibootSignature != MULTIBOOT_BOOTLOADER_MAGIC)
    fatal("Not Multiboot: 0x%08x\n", multibootSignature);

  assert(multibootInfo < 0x40000000u);
  assert(multibootInfo + sizeof(struct MultibootInfo) < 0x40000000u);

  struct MultibootInfo *mbi = PTOKV(multibootInfo, struct MultibootInfo *);

  if (mbi->flags & MBI_MMAP) {
    // printf("Memory map valid: 0x%x %d\n", mbi->mmap_addr, mbi->mmap_length);

    assert(mbi->mmap_addr < 0x40000000u);
    assert(mbi->mmap_addr + mbi->mmap_length < 0x40000000u);

    uint32_t offset = 0;
    while (offset < mbi->mmap_length) {
      struct MultibootMMap *mmap = 
	PTOKV(mbi->mmap_addr + offset, struct MultibootMMap *);

      kpa_t base = mmap->base_addr_high;
      base <<= 32;
      base |= mmap->base_addr_low;

      kpa_t len = mmap->length_high;
      len <<= 32;
      len |= mmap->length_low;

      PmemClass cls = mmap_to_class(mmap->type);
      PmemUse   use = mmap_to_use(mmap->type);

      // printf("[%llx,%llx] multiboot type %x\n", base, base+len, mmap->type);

      pmem_AllocRegion(base, base+len, cls, use, pmc_descrip(cls));

      offset += mmap->size + 4;	/* size field is size of following bytes */
    }
  }
  else if (mbi->flags & MBI_MEMVALID) {
    // printf("General memory valid: %d %d\n", mbi->mem_lower, mbi->mem_upper);

    pmem_AllocRegion(0, mbi->mem_lower * 1024, pmc_RAM, pmu_AVAIL, pmc_descrip(pmc_RAM));
    pmem_AllocRegion(0x100000, mbi->mem_upper * 1024, pmc_RAM, pmu_AVAIL, pmc_descrip(pmc_RAM));
  }
  else
    fatal("Multiboot did not provide memory config\n");

  /*
   * Reserve some memory regions so that they do not get grabbed by
   * the heap backing storage allocator prematurely.
   */

  /* Reserve some additional regions: */
  pmem_AllocRegion(0, 1024, pmc_RAM, pmu_BIOS, "BIOS area");

#if MAX_NCPU > 1
  /* Reserve some additional regions: */
  pmem_AllocRegion(4096, 8192, pmc_RAM, pmu_SMP, "SMP AP boot");
#endif

  {
    /* We need to guard the EBDA region. Not all BIOS memory queries
     * report this region, with the consequence that multiboot itself
     * does not always identify it.
     *
     * If multiboot DOES define it, it will have shown up as
     * "reserved", and therefore will have been reported as a ROM BIOS
     * region. We will actually treat it as ROM, but it's a RAM
     * region.
     *
     * Unfortunately, some BIOS's report an overlap. For example, the
     * HP Pavilion a6030n reports an initial RAM region of [0,0x9f400],
     * a BIOS ROM region of [0x9f400,0x9fc00], and then we get here and
     * discover a claimed EBDA region of [0x9f000,0x9fc00]. Obviously
     * the various people who worked on that BIOS didn't talk to each
     * other. The problem is that we do not know who has it right, and
     * the EBDA may contain various bits of information that we will
     * need later for ACPI. The conservative solution is to ensure
     * that the EBDA gets reserved, which is what we do here.
     *
     * 16-bit segment base of EBDA appears at address 0x403. First
     * byte of the EBDA gives size of EBDA in kilobytes.
     */
    uint32_t ebda_base = (* PTOKV(0x40e, uint16_t *)) << 4;
    uint32_t ebda_size = (* PTOKV(ebda_base, uint8_t *));
    ebda_size *= 1024;
    uint32_t ebda_bound = ebda_base + ebda_size;

    while (ebda_base < ebda_bound) {
      PmemInfo *pmi = pmem_FindRegion(ebda_base);
      uint32_t bound = ebda_bound;
      if (pmi->bound < ebda_bound)
	bound = pmi->bound;

      pmem_AllocRegion(ebda_base, bound, pmc_RAM, pmu_BIOS,
		       "Extended BIOS Data Area");

      ebda_base = bound;
    }

    printf("EBDA = [0x%lx:%lx] (0x%lx)\n", ebda_base, ebda_bound, 
	   ebda_size);
  }

  pmem_AllocRegion(KVTOP(_start), KVTOP(_etext), pmc_RAM, pmu_KERNEL,
		   "Kernel code");
  pmem_AllocRegion(KVTOP(__syscallpg), KVTOP(__esyscallpg),
		   pmc_RAM, pmu_KERNEL,
		   "Syscall page");
  /* Following includes the transmap page(s) */
  pmem_AllocRegion(KVTOP(__begin_maps), KVTOP(__end_maps), pmc_RAM, pmu_KERNEL,
  		   "Kernel mapping pages");
  pmem_AllocRegion(KVTOP(cpu0_kstack_lo), KVTOP(cpu0_kstack_hi), 
		   pmc_RAM, pmu_KERNEL, "CPU0 stack");
  pmem_AllocRegion(KVTOP(_pagedata), KVTOP(_bss_end), pmc_RAM, pmu_KERNEL,
		   "Kernel shared data/bss");

  // printf("config_physical_memory()\n");
}

static bool 
module_option_isstring(const char *cmdline, const char *opt, const char *val)
{
  size_t len = strlen(opt);
  size_t vlen = strlen(val);

  if (cmdline == NULL)
    return false;

  for( ;*cmdline; cmdline++) {
    if (*cmdline != *opt)
      continue;

    if (memcmp(cmdline, opt, len) != 0)
      continue;

    if (cmdline[len] != '=')
      continue;

    cmdline += (len + 1);
    break;
  }

  if (*cmdline == 0)
    return false;

  if (memcmp(cmdline, val, vlen) != 0)
    return false;

  return (cmdline[vlen] == 0 || cmdline[vlen] == ' ');
}

/** @brief Protect multiboot-allocated memory areas from allocation.
 *
 * Record the physical memory locations of modules and command
 * line. This ensures that they are not allocated by the physical
 * memory allocator as we build the heap later.
 */
static void
process_multiboot_info(void)
{
  struct MultibootInfo *mbi = PTOKV(multibootInfo, struct MultibootInfo *);

  /* Copy in the multiboot command line */
  if (mbi->flags & MBI_CMDLINE) {
    /* The multiboot command line is null terminated, but we do not know
     * its length. We aren't prepared to accept more than
     * COMMAND_LINE_LIMIT bytes anyway, so the simple thing to do is
     * just copy COMMAND_LINE_LIMIT bytes starting at mbi-cmdline, and
     * then put a NUL on the end in case the command line supplied by
     * the boot loader was longer. */
    memcpy_ptov(CommandLine, mbi->cmdline, COMMAND_LINE_LIMIT);
    CommandLine[COMMAND_LINE_LIMIT - 1] = 0;
  }

  if (mbi->flags & MBI_MODS) {
    size_t nmods = mbi->mods_count;
  
    const struct MultibootModuleInfo *mmi = 
      PTOKV(mbi->mods_addr, const struct MultibootModuleInfo *);
  
    if (nmods > MAX_MODULES)
      fatal("Too many modules\n");

    for (size_t i = 0; i < nmods; i++, mmi++) {
      const char *cmdline = PTOKV(mmi->string, const char *);

      if (module_option_isstring(cmdline, "type", "load")) {
	ModInfo[i] = *mmi;

	pmem_AllocRegion(mmi->mod_start, mmi->mod_end, 
			 pmc_RAM, pmu_ISLIMG, "module");
	size_t nPages = ((align_up(mmi->mod_end, COYOTOS_PAGE_SIZE) - 
			  align_down(mmi->mod_start, COYOTOS_PAGE_SIZE))
			 / COYOTOS_PAGE_SIZE);
	printf("Module: [0x%08x,0x%08x] %s (%d pages)\n", mmi->mod_start, 
	       mmi->mod_end, cmdline, nPages);
      }
    }
  }
}

/** @brief Release the memory reserve on the provided module. 
 *
 * We can't shrink the module vector, but we want to mark that it has
 * been released, so after releasing we set end=start, because there
 * is no sense reserving memory for a zero-length module.
 */
static void
release_module(struct MultibootModuleInfo *mmi)
{
  if (mmi->mod_start == mmi->mod_end)
    return;

  pmem_AllocRegion(mmi->mod_start, mmi->mod_end, 
		   pmc_RAM, pmu_AVAIL, pmc_descrip(pmc_RAM));

  /* Mark it released. */
  mmi->mod_end = mmi->mod_start;
}

/** @brief Transfer the content of a module into the object cache. */
static void
load_module(struct MultibootModuleInfo *mmi)
{
  const char *cmdline = PTOKV(mmi->string, const char *);
  printf("Loading: [0x%08x,0x%08x] %s\n", mmi->mod_start, 
	 mmi->mod_end, cmdline);

  const char *imgName = cmdline_argv0();

  cache_preload_image(imgName, mmi->mod_start,
		      mmi->mod_end - mmi->mod_start);
}

/** @brief Transfer module content into the object heap.
 *
 * Move object content into the object vectors. Once this is done we
 * release the transferred module images.
 *
 * @bug The test for exactly one module isn't correct. Not immediately
 * clear what the correct test ought to be. This is a design issue
 * rather than a code issue: what other modules might we want here,
 * and how should they be differentiated?
 */
static void
process_modules(void)
{
  size_t nPreload = 0;

  for (size_t i = 0; i < MAX_MODULES; i++) {
    if (ModInfo[i].mod_start == 0)
      continue;

    nPreload++;
    load_module(&ModInfo[i]);
    release_module(&ModInfo[i]);
  }

  if (nPreload == 0)
    fatal("No object preload modules!\n");
}

/**
 * @brief Perform architecture-specific initialization.
 *
 * arch_init() is the first thing called from the Coyotos kern_main()
 * procedure. On the Pentium, the situation at the time kern_main() is
 * entered is that we have a valid virtual map, and we are running in
 * 32-bit protected mode (courtesy of Grub), but we haven't got our
 * own GDT or LDT or IDT loaded. Fixing this is the first order of
 * business, because we will start missing clock interrupts if we
 * don't get this done quickly.
 *
 * @section init_theory Initialization: Theory of Operation
 *
 * The order of initialization is critical and delicate. Here is a
 * quick explanation.
 *
 * On startup, boot.S has either left us in PAE or legacy translation
 * mode (according to <tt>UsingPAE</tt>) and has constructed a 1:1 map
 * of the V[0:2M-1] => P[0:2M-1] and also V[3G,3G+2M-1] => P[0:2M-1].
 * More precisely, it built one leaf page table worth of mappings, and
 * has recorded this in two places in the page directory. In "legacy
 * PTE" mode this is actually a [0:4M-1] region, but we shouldn't make
 * assumptions about this. One of our goals is going to be to abandon
 * this mapping fairly quickly, but note that we do need to be careful
 * about references into memory above 2M until we can deal with this.
 *
 * <b>Order of Bringup</b>
 *
 * <ul>
 * <li>
 * We will want to do diagnostics as early as possible, so bring up
 * the console first.  The console uses only statically preallocated
 * storage, and we are taking advantage of the fact that the BIOS has
 * already initialized the video subsystem for us (on other boards we
 * might need to init the display and deal with font issues here).
 *
 * The kernel will not own the console forever. Just before
 * transferring control to the first non-kernel code we will disable
 * further use of the display.
 * </li>
 * <li>
 * Call <tt>pmem_init()</tt>, establishing the total addressable
 * physical range. This creates the initial "hole" from which all
 * physical ranges are created.
 *
 * As with the console, the number of physical memory ranges is
 * statically fixed at compile time. No memory allocation is done
 * here.
 * </li>
 * <li>
 * Configure physical memory. That is: we scan the
 * multiboot-supplied list of memory regions and tell the PhysMem
 * allocator about them. Note that PhysMem is still using statically
 * allocated entries, so we aren't stepping on anything here.
 *
 * At the end of this phase we pre-allocate the regions containing the
 * BIOS area, the Extended BIOS Data Area, the kernel code/data, and
 * the CPU0 stack. The purpose of this preallocation is to ensure that
 * nothing steps on these regions in later allocations.
 *
 * At the end of this stage we compute an initial estimate of the
 * total number of physical pages available to us.
 * </li>
 * <li>
 * Grab a snapshot of the total number of available physical pages.
 * This will guide us later when we allocate various caches.
 * </li>
 * <li>
 * Protect the modules from interference until we can deal with them
 * properly. We do this by marking them allocated in the physical
 * range map.
 * </li>
 * <li>
 * Process command line arguments, and protect the command line itself
 * from interference by pre-allocating its physical memory location.
 * We need to do this because GRUB does not tell us about the regions
 * that it has dynamically allocated for its own structures.
 * </li>
 * <li>
 * Allocate the kernel heap. Once this is done we can do dynamic
 * allocations for various things, notably to save the command line in
 * a safe place.
 * </li>
 * <li>
 * Reserve memory space for page tables.
 * </li>
 * <li>
 * Scan CPUs to figure out how many we have and allocate the necessary
 * per-CPU structures and per-CPU private mappings.
 * </li>
 * <li>
 * Set up GDT, LDT, TSS, and IRQ tables.
 * </li>
 * </ul>
 *
 * We now return to the main-line code so that it can initialize the
 * object cache. It will call arch_cache_init() to let us populate the
 * object cache and release module storage.
 */
void 
arch_init(void)
{
  /* Initialize the console output first, so that we can get
   * diagnostics while the rest is running.
   */
  console_init();

  /* Initialize the CPU structures, since we need to call things that
   * want to grab mutexes.
   */
  for (size_t i = 0; i < MAX_NCPU; i++)
    cpu_construct(i);

  init_gdt();
  init_tss();

  /* Initialize the transient map, so that later code can access
     arbitrary parts of memory. */
  transmap_init();

#if 0
  {
    uint32_t cr4;
    GNU_INLINE_ASM("mov %%cr4,%0\n"
		   : "=q" (cr4));
    if (cr4 & CR4_PAE)
      printf("We appear to be running in PAE mode.\n");
    else
      printf("Running with legacy mapping.\n");
  }
#endif

  printf("IA32_UsingPAE: %s\n", IA32_UsingPAE ? "yes" : "no");

  pmem_init(0, IA32_UsingPAE ? PAE_PADDR_BOUND : PTE_PADDR_BOUND);

  /* Need to get the physical memory map before we do anything else. */
  config_physical_memory();

  /* Need to get this estimate BEFORE we protect the multiboot
   * regions, because we are going to release those back into the pool
   * later, and if we don't count them now we will end up
   * under-supplied with header structures for them.
   */
  size_t totPage = 
    pmem_Available(&pmem_need_pages, COYOTOS_PAGE_SIZE, false);

  /* Make sure that we don't overwrite the loaded modules during cache
   * initialization.
   */
  process_multiboot_info();

  cmdline_process_options();

  if (cmdline_has_option("noioapic"))
    use_ioapic = false;
  else if (cmdline_has_option("ioapic"))
    use_ioapic = true;

  /* Find all of our CPUs. Also checks the ACPI tables, which we
     should probably do separately. Probing the ACPI tables may have
     the side effect of updating the execption vector table as we
     discover interrupt sources. */
  (void) cpu_probe_cpus();

  printf("%d pages initially available\n", totPage);

  {
    /* heap_base could nominally start at _end, but if we round it up
     * we can guarantee that it ends up on the next page table. This
     * is advantageous, because page table 0 is the CPU-private page
     * table. Moving all of the heap mappings above this means that we
     * won't take cross-CPU faults to propagate page table entries
     * when the heap grows. */
    kva_t align = IA32_UsingPAE ? 2048*1024 : 4096*1024;
    kva_t heap_base = align_up((kva_t)&_end, align);
    heap_init(heap_base, heap_base, HEAP_LIMIT_VA);
  }

  {
    size_t nPage = totPage - RESERVED_PAGE_TABLES(totPage);
    nPage -= (cpu_ncpu-1) * KSTACK_NPAGES; /* per-CPU stacks */

    cache_estimate_sizes(PAGES_PER_PROCESS, nPage);
  }

  pagetable_init();

  //  pmem_showall();

  cpu_scan_features();

  /* Initialize the hardware exception vector table. */
  vector_init();
  
  printf("CPU0: GDT/LDT, ");
  load_gdtr_ldtr();		/* for CPU 0 */

  printf("TSS, ");
  load_tr();

  irq_init();			/* for CPU 0 */

  printf(" established\n");

  cpu_init_all_aps();

  //  pmem_showall();

  // printf("Test of breakpoint:\n");
  // GNU_INLINE_ASM ("int3");

  hardclock_init();

  printf("Enabling interrupts\n");
  GNU_INLINE_ASM ("sti");
}

void 
arch_cache_init()
{
  process_modules();

  cache_add_page_space(true);
}
