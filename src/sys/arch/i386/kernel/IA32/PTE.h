#ifndef __IA32_PTE_H__
#define __IA32_PTE_H__
/*
 * Copyright (C) 1998, 1999, The EROS Group, LLC.
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
 *
 * @brief Bits of x86 page table entry.
 *
 * I have intentionally left out the PAT bit, because it moves
 * depending on whether you are doing 2Mbyte pages or 4K pages. Turns
 * out the bit is irregular in the legacy mode too, and I never
 * noticed because I've never seen any good reason to use it on a
 * normal processor.
 *
 * Note that the G bit and the D bit have *never* been meaningful at
 * any level other than leaf entries, in spite of what the diagrams
 * would have you believe. The correct policy for these bits is only
 * to set them in leaf entries. The historical practice in EROS was
 * always to set the D bit on a writable page in order to suppress the
 * TLB write-back. For the moment, we continue that practice in
 * Coyotos, though I am no longer sure that we should
 *
 * Unfortunately, the A bit is updated by the TLB hardware to indicate
 * that the page table or page beneath has been referenced. Even
 * worse, the A bit is not implemented in regular fashion at all
 * levels under PAE. In EROS I simply preset the A bit at all
 * levels. In Coyotos I think that I will only do this bit at the leaf
 * level and eat the update costs at all other levels, since it won't
 * be visible for benchmarking purposes and it shouldn't impact
 * steady-state behavior much.
 */

/* Local Variables: */
/* comment-column:24 */
/* End: */

#define PTE_V	 0x001	/**< @brief present (a.k.a valid) */
#define PTE_W    0x002	/**< @brief writable */
#define PTE_USER 0x004	/**< @brief user-accessable page */
#define PTE_PWT  0x008	/**< @brief page write through */
#define PTE_PCD  0x010	/**< @brief page cache disable */
#define PTE_ACC  0x020	/**< @brief accessed */
#define PTE_DRTY 0x040	/**< @brief dirty */
#define PTE_PGSZ 0x080  /**< @brief large page (PDE, >=Pentium only) */
#define PTE_GLBL 0x100  /**< @brief global page (PDE,PTE, >= PPro only) */
#define PTE_SW0  0x200	/**< @brief SW use */
#define PTE_SW1  0x400	/**< @brief SW use */
#define PTE_SW2  0x800	/**< @brief SW use */

#define PTE_FRAMEBITS 0xfffff000
#define PTE_INFOBITS  0x00000fff /**< @brief Permissions fields mask */

/* The PCD, PWT bits are also available in CR3 in both normal and PAE
   modes. */

/* There is a problem here. PAE is a 64-bit data structure, and when
 * we are running from assembler on a 32-bit platform, we cannot
 * reference the higher bits cleanly. On the other hand, I
 * provisionally want to reuse the same header file for EM64T amd
 * AMD64 platforms. The end result is that I am relying on
 * zero-extension here and not defining the lower bits as 64-bit
 * quantities.
 *
 * As described above, the G, D bits have historically never been
 * meaningful above the leaf level. The A bit was meaningful at all
 * levels where it was implemented, but some genious failed to
 * implement in the PDPT layer in 32-bit PAE. This is survivable. We
 * just initialize it to zero at all layers except the leaf, and
 * initialize it to 1 in the leaf to suppress TLB writeback. The worst
 * that will happen is that the processor will update the
 * mid-layer. Alternatively, we can check the kernel tables to see
 * what is actually getting updated by the current CPU, and use that
 * information to determine what to do. Presumably the processor won't
 * barf when reloading the bits that it has previously set.
 *
 * On IA32e, the A bit is uniform at all levels. No problem there,
 * since we know we are in 64-bit mode in that case.
 *
 * But here is the priceless, unbelievably stupid, completely takes
 * the cake, utterly idiotic one: the U/S bit and the R/W bit are not
 * implemented in the PDPT layer on the legacy machines AND MUST BE
 * ZERO, but they *are* implemented in IA32e and BOTH BITS ARE HIGHLY
 * LIKELY TO NEED TO BE 1!!!
 *
 * The final outcome of all this is that there is absolutely no way to
 * avoid setting the U/S and R/W bits in a level-sensitive way when
 * running under 32-bit PAE mode. Hmm. I need to find out if these
 * reserved bits were actually *checked* on the older processors. We
 * might get away with it even though it is not supposed to be done.
 *
 * Now for some good news: while we can't really make any use of the
 * NX bit from the early stage bootstrap in PAE mode on IA32, it
 * doesn't really matter, because we don't need to set that bit early.
 */

#define PAE_V	 0x001 /**< @brief present (a.k.a valid) */
#define PAE_W    0x002 /**< @brief writable */
#define PAE_USER 0x004 /**< @brief user-accessable page */
#define PAE_PWT  0x008 /**< @brief page write through */
#define PAE_PCD  0x010 /**< @brief page cache disable */
#define PAE_ACC  0x020 /**< @brief accessed */
#define PAE_DRTY 0x040 /**< @brief dirty */
#define PAE_PGSZ 0x080 /**< @brief large page */
#define PAE_GLBL 0x100 /**< @brief global page */
#define PAE_SW0  0x200 /**< @brief SW use */
#define PAE_SW1  0x400 /**< @brief SW use */
#define PAE_SW2  0x800 /**< @brief SW use */
#ifndef __ASSEMBLER__
#define PAE_EXB  0x10000000ll
#endif

#endif /* __IA32_PTE_H__ */
