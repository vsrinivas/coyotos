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
#include <kerninc/ccs.h>
#include <kerninc/malloc.h>
#include <kerninc/util.h>
#include <kerninc/Cache.h>
#include <kerninc/CPU.h>
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
extern uint32_t __cpu_data_start[];
extern uint32_t __cpu_data_end[];
extern uint32_t _pagedata[];
extern uint32_t _end[];
extern uint32_t _bss_end[];
extern uint32_t __begin_maps[];
extern uint32_t __end_maps[];
extern uint32_t kstack_lo[];
extern uint32_t kstack_hi[];

/** @brief For each CPU, a pointer to the local per-CPU structure.
 *
 * This must be re-initialized by each CPU as it creates its
 * local copy of per-CPU state. 
 */
DEFINE_CPU_PRIVATE(CPU*,curCPU) = &cpu_vec[0];

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

/** @brief Return the argv[0] element of a command line. */
static const char *
cmdline_argv0(const char *cmdline)
{
  while (isspace(*cmdline))
    cmdline++;

  if (*cmdline)
    return cmdline;

  fatal("Command line %s had no argv[0]\n", cmdline);
}

static char
cmdline_addnull(const char *item)
{
  while (*item && !isspace(*item))
    item++;

  char c = *item;
  *((char *) item) = 0;

  return c;
}

static void
cmdline_fixnull(const char *item, char c)
{
  while (*item)
    item++;
  *((char *) item) = c;
}

/** @brief Given a command line, find the named option if present, or
 * return NULL.
 */
static const char *
cmdline_find_option(const char *cmdline, const char *optname)
{
  size_t len = strlen(optname);

  while (isspace(*cmdline))
    cmdline++;

  /* Skip argv[0], which is kernel or module name. */
  while (*cmdline && !isspace(*cmdline))
    cmdline++;


  /* Hunt for named option */
  while (*cmdline) {
    if (isspace(*cmdline)) {
      cmdline++;
      continue;
    }

    const char *opt = cmdline;
    while (*cmdline && *cmdline != '=' && !isspace(*cmdline))
      cmdline++;

    if ( ((cmdline - opt) == len) && (memcmp(opt, optname, len) == 0) )
      return opt;

    /* Not the desired option. Skip to next candidate: */
    while (*cmdline && !isspace(*cmdline))
      cmdline++;
  }

  return NULL;
}

/** @brief Return pointer to option value string, if present, else
    return @p default. */
static const char *
cmdline_option_arg(const char *option)
{
  while (*option && *option != '=')
    option++;

  if (*option != '=')
    return NULL;
  option++;

  return option;
}

/** @brief return true if option exists, has a value, and value
 *  matches candidate, else false. */
static bool
cmdline_option_isstring(const char *cmdline, const char *optname, 
			const char *value)
{
  const char *option = cmdline_find_option(cmdline, optname);
  if (!option)
    return false;

  option = cmdline_option_arg(option);
  if (!option)
    return false;

  size_t len = strlen(value);
  const char *optend = option;

  while (*optend && !isspace(*optend))
    optend++;

  if ( ((optend - option) == len) &&
       memcmp(option, value, len) == 0 )
    return true;

  return false;
}

/** @brief return non-zero if option exists and has an integral value,
 *  else 0. */
static unsigned long
cmdline_option_uvalue(const char *cmdline, const char *optname)
{
  const char *option = cmdline_find_option(cmdline, optname);
  if (!option)
    return 0;

  option = cmdline_option_arg(option);
  if (!option)
    return 0;

  return strtoul(option, 0, 0);
}

/** @brief When "dbgwait" is set on the command line, we set this variable to
 * 1.  We then spin, waiting for the debugger to clear it.
 */
volatile uint32_t debugger_wait = 0;

static void
process_dbgwait(void)
{
  printf("waiting for debugger to clear debugger_wait\n");
  debugger_wait = 1;
  while (debugger_wait == 1)
    GNU_INLINE_ASM ("nop");
  printf("dbgwait complete");
}

/** @brief Process any command line options.
 *
 * Pick off any command line options that we care about and then
 * release the hold on the memory that the command line occupies.
 */
static void
process_command_line()
{
  // We have already validated that this was a multiboot boot above.
  struct MultibootInfo *mbi = PTOKV(multibootInfo, struct MultibootInfo *);

  if ((mbi->flags & MBI_CMDLINE) == 0)
    return;

  char *cmdline = PTOKV(mbi->cmdline, char *);

  Cache.c_Process.count = cmdline_option_uvalue(cmdline, "nproc");
  Cache.c_GPT.count = cmdline_option_uvalue(cmdline, "ngpt");
  Cache.c_Endpoint.count = cmdline_option_uvalue(cmdline, "nendpt");
  //   Cache.page.count = cmdline_option_uvalue(cmdline, "npage");
  Cache.dep.count = cmdline_option_uvalue(cmdline, "depend");

  if (cmdline_find_option(cmdline, "dbgwait") != 0)
    process_dbgwait();

  if (cmdline_find_option(cmdline, "apic") != 0)
    use_apic = true;
}

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
     * In this case, the call to pmem_AllocRegion below succeeds on
     * all platforms we know about because it exactly overlaps the
     * previously defined ROM region (and therefore replaces it
     * without signalling any error).
     *
     * This is sloppy. They should be kpa_t, but we know the uint32_t
     * span will work, and declaring them this way avoids cast
     * complaints.
     */
    kpa_t ebda_base = (* PTOKV(0x40e, uint16_t *)) << 4;
    kpa_t ebda_bound = (* PTOKV(ebda_base, uint32_t *)) * 1024;
    ebda_bound += ebda_base;

    //    fatal("EBDA = [0x%x:0x%x]\n", ebda_base, ebda_bound);

    // ebda_base = align_down(ebda_base, COYOTOS_PAGE_SIZE);
    pmem_AllocRegion(ebda_base, ebda_bound, pmc_RAM, pmu_BIOS, 
    		     "Extended BIOS Data Area");
  }

  pmem_AllocRegion(KVTOP(_start), KVTOP(_etext), pmc_RAM, pmu_KERNEL,
		   "Kernel code");
  pmem_AllocRegion(KVTOP(__syscallpg), KVTOP(__esyscallpg),
		   pmc_RAM, pmu_KERNEL,
		   "Syscall page");
  pmem_AllocRegion(KVTOP(__cpu_data_start), 
		   KVTOP(__cpu_data_end),
		   pmc_RAM, pmu_KERNEL,
		   "CPU private data");
  pmem_AllocRegion(KVTOP(__begin_maps), KVTOP(__end_maps), pmc_RAM, pmu_KERNEL,
  		   "Kernel mapping pages");
  pmem_AllocRegion(KVTOP(kstack_lo), KVTOP(kstack_hi), pmc_RAM, pmu_KERNEL,
  		   "Kernel stack");
  pmem_AllocRegion(KVTOP(_pagedata), KVTOP(_bss_end), pmc_RAM, pmu_KERNEL,
		   "Kernel shared data/bss");

  // printf("config_physical_memory()\n");
}

/** @brief Protect multiboot-allocated memory areas from allocation.
 *
 * Record the physical memory locations of modules and command
 * line. This ensures that they are not allocated by the physical
 * memory allocator as we build the heap later.
 */
static void
protect_multiboot_regions(void)
{
  struct MultibootInfo *mbi = PTOKV(multibootInfo, struct MultibootInfo *);

  if (mbi->flags & MBI_MODS) {
    size_t nmods = mbi->mods_count;
  
    const struct MultibootModuleInfo *mmi = 
      PTOKV(mbi->mods_addr, const struct MultibootModuleInfo *);
  
    while (nmods) {
      const char *cmdline = PTOKV(mmi->string, const char *);
      pmem_AllocRegion(mmi->mod_start, mmi->mod_end, 
		       pmc_RAM, pmu_ISLIMG, "module");
      size_t nPages = ((align_up(mmi->mod_end, COYOTOS_PAGE_SIZE) - 
			align_down(mmi->mod_start, COYOTOS_PAGE_SIZE))
		       / COYOTOS_PAGE_SIZE);
      printf("Module: [0x%08x,0x%08x] %s (%d pages)\n", mmi->mod_start, 
	     mmi->mod_end, cmdline, nPages);
      nmods--;
      mmi++;
    }
  }

  if (mbi->flags & MBI_CMDLINE) {
    char *cmdline = PTOKV(mbi->cmdline, char *);

    /* Preserve the command line for later use: */
    size_t cmdline_alloc = strlen(cmdline) + 1; /* include NUL */

    pmem_AllocRegion(mbi->cmdline, mbi->cmdline + cmdline_alloc, 
		     pmc_RAM, pmu_KERNEL, "command line");
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

  const char *imgName = cmdline_argv0(cmdline);
  char old = cmdline_addnull(imgName);

  cache_preload_image(imgName, mmi->mod_start,
		      mmi->mod_end - mmi->mod_start);

  cmdline_fixnull(imgName, old);
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
  struct MultibootInfo *mbi = PTOKV(multibootInfo, struct MultibootInfo *);

  size_t nPreload = 0;

  size_t nmods = mbi->mods_count;

  struct MultibootModuleInfo *mmi = 
    PTOKV(mbi->mods_addr, struct MultibootModuleInfo *);
  
  printf("Processing %d module%s\n", mbi->mods_count, 
	 ((mbi->mods_count > 1) ? "s": ""));

  for (size_t i = 0; i < nmods; i++) {
    struct MultibootModuleInfo *thisModule = &mmi[i];

    const char *cmdline = PTOKV(thisModule->string, const char *);
    const char *option = cmdline_find_option(cmdline, "type");
    if (!option)
      continue;

    if (cmdline_option_isstring(cmdline, "type", "load")) {
      nPreload++;
      load_module(thisModule);
    }

    release_module(thisModule);
  }

  if (nPreload == 0)
    fatal("No object preload modules!");
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
  /* Initialize the transient map, so that later code can access
     arbitrary parts of memory. */
  transmap_init();

  /** @bug Should be able to turn off low kernel map here. */

  // Initialize the console output first, so that we can get
  // diagnostics while the rest is running.
  console_init();

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

  printf("UsingPAE: %s\n", UsingPAE ? "yes" : "no");

  pmem_init(0, UsingPAE ? PAE_PADDR_BOUND : PTE_PADDR_BOUND);

  /* Need to get the physical memory map before we do anything else. */
  config_physical_memory();

  (void) cpu_probe_cpus();

  /* Need to get this estimate BEFORE we protect the multiboot
   * regions, because we are going to release those back into the pool
   * later, and if we don't count them now we will end up
   * under-supplied with header structures for them.
   */
  size_t totPage = 
    pmem_Available(&pmem_need_pages, COYOTOS_PAGE_SIZE, false);

  printf("%d pages initially available\n", totPage);

  // Make sure that we don't overwrite the loaded modules during cache
  // initialization.
  protect_multiboot_regions();

  {
    /* heap_base could nominally start at _end, but if we round it up
     * we can guarantee that it ends up on the next page table. This
     * is advantageous, because page table 0 is the CPU-private page
     * table. Moving all of the heap mappings above this means that we
     * won't take cross-CPU faults to propagate page table entries
     * when the heap grows. */
    kva_t align = UsingPAE ? 2048*1024 : 4096*1024;
    kva_t heap_base = align_up((kva_t)&_end, align);
    heap_init(heap_base, heap_base, HEAP_LIMIT_VA);
  }

#if 0
  /* Every CPU beyond the first one is going to need a per-cpu page
     directory and a per-cpu page table for CPU-local storage: */
  totPage -= (2 * cpu_ncpu);

  /* The directory window will require page tables containing 16,384
     total slots: */
  totPage -= (16384 / (UsingPAE ? NPAE_PER_PAGE : NPTE_PER_PAGE));

  /* We will use 10% of page frames for page tables. This number is
   * fairly arbitrary. */
  totPage -= (totPage/10);
#endif

  process_command_line();

  {
    size_t nPage = totPage - RESERVED_PAGE_TABLES(totPage);
    nPage -= (cpu_ncpu-1) * 2;	/* per-CPU private tables */

    cache_estimate_sizes(PAGES_PER_PROCESS, nPage);
  }

  pagetable_init();

  cpu_vector_init();

  //  pmem_showall();

  irq_vector_init();

  cpu_scan_features();

  printf("CPU0: GDT/LDT, ");
  gdt_ldt_init();		/* for CPU 0 */

  printf("TSS, ");
  tss_init();

  irq_init();			/* for CPU 0 */

  printf(" established\n");

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
