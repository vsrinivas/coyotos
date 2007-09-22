/* -*- text -*- */
/** @file
 * @brief Linker directives for x86 kernel linkage.
 */
#include <coyotos/i386/pagesize.h>
#include <hal/config.h>

ENTRY(_start)

/* OUTPUT_FORMAT:
               default,     big-endian,   little-endian) */
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386") 
OUTPUT_ARCH("i386")
TARGET("elf32-i386")

SECTIONS
{
  . = KVA + 0x100000;  /* Kernel start at 1MB */

  /* TEXT REGION (text + rodata) */
  _text = .;
  .text : 
      AT( ADDR(.text) - KVA )
      { *(.text) } = 0x9090
  _etext = .;

  _rodata = .;
  .rodata :
      AT( ADDR(.rodata) - KVA )
      { *(.rodata) *(.rodata.*) }
  _erodata = .;

  /* SYSCALL TRAMPOLINE PAGE */
  . = ALIGN(COYOTOS_PAGE_SIZE);
  __syscallpg = .;
  .sc_trampoline : 
      AT( ADDR(.sc_trampoline) - KVA )
      { *(.sc_trampoline)
        . = ALIGN(COYOTOS_PAGE_SIZE);
      } = 0x9090
  __esyscallpg = .;

  /* This next part is a bit subtle. In Coyotos, CPU-local storage is
   * in fact CPU *private*. All CPUs see their private storage at the 
   * same virtual address. This helps us ensure isolation between
   * CPUs more effectively. It improves code generation, because
   * we do not need to maintain a global register to point to
   * CPU-local storage. Finally, it makes multiprocessing possible, 
   * because on IA-32 we don't have a large enough kernel virtual
   * region to be able to cross-map things in any case.
   *
   * A consequence of this design choice is that CPUs can never share
   * a page directory. There is at least one page table in the kernel
   * region that is CPU-private, and therefore at least one entry in the
   * page directory that is CPU-private.
   *
   * On PAE, recall that the entire kernel virtual region is <1G, so
   * there is only *one* CPU-private page directory on PAE.
   *
   * In arranging the order of data below, the goal is to get all of
   * the CPU-private data gathered in such a way that it all falls
   * within the leading 2Mbytes. If we can do this, then it will all
   * fall within the leading page table, even on PAE).
   *
   * The other option would be to explicitly gather all of this stuff
   * into a separately aligned and loaded region. In many respects I
   * would prefer that, but it makes setting up the initial page
   * tables harder, and it appears that GRUB will not load any
   * executable having more than two loadable sections.
   */

  . = ALIGN(COYOTOS_PAGE_SIZE);
  /* Everything mapped between _percpu_start and _percpu_end is
   * CPU-private.
   */

  __cpu_start = .;
  __cpu_data_start = .;
  .data.percpu :
      AT( ADDR(.data.percpu) - KVA )
      {
        *(.data.percpu) 
        . = ALIGN(COYOTOS_PAGE_SIZE);
      }
  __cpu_data_end = .;

  . = ALIGN(COYOTOS_PAGE_SIZE);
  __begin_maps = .;

  /* CPU0 Page Directory and Page Table. Other CPUs will have their
   * CPU-specific page directory and page table appear at the same
   * virtual addresses.
   *
   * The CPU0 page directory and page table will serve double duty as
   * the *master* page directory and page table, for which purpose these
   * pages will be given secondary mapping addresses at
   * MASTER_PAGE_DIRECTORY_VA and MASTER_PAGE_TABLE_VA respectively.
   */
   /*
   * The ones below are used for CPU0. Subsequent CPUs are set up by
   * hand in the architecture-dependent initialization logic. Note
   * that all CPUs use the same virtual address for their stack, which
   * is /kstack_hi/.
   */
  KernPageDir = .;
  . = . + COYOTOS_PAGE_SIZE;

  /* Kernel Page Table for first 2M (PAE) or 4M (Legacy) */
  . = ALIGN(COYOTOS_PAGE_SIZE);
  KernPageTable = .;
  . = . + COYOTOS_PAGE_SIZE;


  /* Page table for the CPU0 transient mappings page. This location
   * will be re-mapped to a private page on each CPU. */
  TransientMap = .;
  . = . + COYOTOS_PAGE_SIZE;
  __end_maps = .;

  /* Note that we are already aligned to page size here, but just in case: */
  . = ALIGN(KSTACK_SIZE);
  kstack_lo = .;
  . = . + KSTACK_SIZE;
  kstack_hi = .;

  /* PAGE DATA REGION (all data whose size is a page multiple and 
     is page aligned) */
  . = ALIGN(COYOTOS_PAGE_SIZE);
  _pagedata = .;
  .pagedata :
      AT( ADDR(.pagedata) - KVA )
      {
        *(.pagedata)
      }
  _epagedata = .;

  . = ALIGN(COYOTOS_PAGE_SIZE);
  _data = .;
  /* NORMAL DATA */
  . = ALIGN(CACHE_LINE_SIZE);
  .data.cachealign :
      AT( ADDR(.data.cachealign) - KVA )
      {
        *(.data.cachealign) 
        . = ALIGN(CACHE_LINE_SIZE);
      }
  .data : 
      AT( ADDR(.data) - KVA )
      {
        *(.data)
      }
  _edata = .;

  /* UNINITIALIZED DATA */
  _bss_start = .;
  .bss  :
      AT( ADDR(.bss) - KVA )
      { *(.bss) }

  _bss_end = .;


  _end = .;


/* What we would need for C++ support:

  __CTOR_LIST__ = .;
  LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)
  *(.ctors)
  LONG(0)
  __CTOR_END__ = .;
  __DTOR_LIST__ = .;
  LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)
  *(.dtors)
  LONG(0)
  __DTOR_END__ = .;
*/

  /* Debugging sections */
  .stab 0 :          { *(.stab) }
  .stabstr 0 :       { *(.stabstr) }
  .stab.excl 0 :     { *(.stab.excl) }
  .stab.exclstr 0 :  { *(.stab.exclstr) }
  .stab.index 0 :    { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment 0 :       { *(.comment) }

  /* Following is for C++ exception support, and really should not
     be getting included at all, but I don't have time to sort out
     how to ignore it at the moment. */
  /DISCARD/ :        { *(.eh_frame) }
}
