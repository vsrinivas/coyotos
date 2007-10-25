/* -*- text -*- */
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

  . = ALIGN(COYOTOS_PAGE_SIZE);
  __begin_maps = .;

  /* Kernel master page directory and page table. This page directory
   * is used when no per-process mapping structure is appropriate.
   */
  KernPageDir = .;
  . = . + COYOTOS_PAGE_SIZE;

  /* Kernel Page Table for first 2M (PAE) or 4M (Legacy) */
  . = ALIGN(COYOTOS_PAGE_SIZE);
  KernPageTable = .;
  . = . + COYOTOS_PAGE_SIZE;


  /* Page tables for the transient mapping region. In PAE mode, each
   * mapping page holds 512 entries, so each table can support up to
   * (512/TRANSMAP_ENTRIES_PER_CPU) CPUs. */
  TransientMap = .;
   . += COYOTOS_PAGE_SIZE * TRANSMAP_PAGES;
  __end_maps = .;

  /* Note that we are already aligned to page size here, but just in case: */
  . = ALIGN(KSTACK_SIZE);
  cpu0_kstack_lo = .;
  . = . + KSTACK_SIZE;
  cpu0_kstack_hi = .;

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
