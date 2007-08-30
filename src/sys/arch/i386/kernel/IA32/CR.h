#ifndef __IA32_CR_H__
#define __IA32_CR_H__
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
 * @brief control register definitions.
 *
 * CR0 is the original control register.<br>
 * CR1 is wholly reserved<br>
 * CR2 is page fault linear address<br> 
 * CR3 is page directory base register
 */

/* CR0 Normal Coyotos State:
  
   PG=1 CD=0 NW=0 AM=1 WP=1 NE=1 ET=1 TS=1 EM=0  MP=1 PE=1

   Setting of NE will vary according to who has the numerics unit
   at any given time. */

#define CR0_PG         0x80000000 /**< @brief enable paging */
#define CR0_CD         0x40000000 /**< @brief cache disable */
#define CR0_NW         0x20000000 /**< @brief not write through */
#define CR0_AM         0x00040000 /**< @brief alignment mask */
#define CR0_WP         0x00010000 /**< @brief write protect */
#define CR0_NE         0x00000020 /**< @brief numeric error */
#define CR0_ET         0x00000010 /**< @brief extension type */
#define CR0_TS         0x00000008 /**< @brief task switched */
#define CR0_EM         0x00000004 /**< @brief numerics emulation */
#define CR0_MP         0x00000002 /**< @brief monitor coprocessor */
#define CR0_PE         0x00000001 /**< @brief protection enable */

/* CR3 Normal Coyotos State: PWT = 0 PCD=0 */

#define CR3_PWT        0x00000004 /**< @brief Page-level write transparent */
#define CR3_PCD        0x00000008 /**< @brief Page-level cache disable */

/* CR4 Normal Coyotos State:

   VME=0 PVI=0 TSD=0 DE=1 PSE=1 PAE=0 MCE=0 PGE=1 PCE=0
   OSFXSR=0 OSXMMEXCPT=0

   Notes on specific selections:

   DE=0  will break compatibility with some programs. Tough.

   PAE=0 may change in the future, I just don't want to deal with that
         at this time.

   MCE=0 is temporary -- we need to handle this.

   PCE=0 Enabling app-level access to performance monitoring raises
         a boat load of security concerns. I'ld actually like to
         turn off RDTSC as well, but that's not a pragmatically
         sustainable design decision.

   OSFXSR=0, OSXMMEXCPT=0 is a bug. I simply haven't implemented SIMD
         support yet.
 */

   
#define CR4_VME        0x00000001 /**< @brief virtual 8086 extensions */
#define CR4_PVI        0x00000002 /**< @brief prot mode virtual interrupts */
#define CR4_TSD        0x00000004 /**< @brief time stamp disable */
#define CR4_DE         0x00000008 /**< @brief debugging extensions */
#define CR4_PSE        0x00000010 /**< @brief page size extensions */
#define CR4_PAE        0x00000020 /**< @brief physical address extension */
#define CR4_MCE        0x00000040 /**< @brief machine check enable */
#define CR4_PGE        0x00000080 /**< @brief page global enable */
#define CR4_PCE        0x00000100 /**< @brief perf. monitor counter enable */
#define CR4_OSFXSR     0x00000200 /**< @brief OS supports FXSAVE/FSTSTOR */
#define CR4_OSXMMEXCPT 0x00000400 /**< @brief OS supports SIMD FP exceptions */

#endif /* __IA32_CR_H__ */
