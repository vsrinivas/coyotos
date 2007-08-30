#ifndef __COLDFIRE_UPCB_H__
#define __COLDFIRE_UPCB_H__

/*
 * Copyright (C) 2006, Jonathan S. Shapiro.
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

#include <hal/kerntypes.h>

/** @file
 * @brief User-mode process control block
 *
 * These structures describe the save area layouts for the processor.
 * These structures describe the ``public'' layout.
 *
 * Individual
 * structures in the UPCB are not guaranteed to be populated unless
 * the necessary flush operations have been performed on the
 * kernel-cached process state.
 */

typedef struct coldfire_UPCB {
  /** @brief Integer register save area.
   *
   * This structure describes the layout within the process structure of
   * the integer register save area. This is <em>not</em> the layout
   * that is used for the corresponding get/set registers routines.
   */
  struct {
    uint32_t d0;
    uint32_t d1;
    uint32_t d2;
    uint32_t d3;
    uint32_t d4;
    uint32_t d5;
    uint32_t d6;
    uint32_t d7;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;		/* a.k.a. stack pointer */
    uint32_t pc;
    uint16_t ccr;		/* condition codes -- stored in SR */

    // Following are supervisor-mode registers, here temporarily for
    // later reference:

    //    uint32_t sr;          /* status register */
    //    uint32_t other_a7;    /* supervisor stack pointer */
    //    uint32_t vbr;		/* vector base register */
    //    uint32_t cacr;		/* cache control register */
    //    uint32_t asid;		/* addres space ID register */
    //    uint32_t acr0;		/* access control register 0 (data) */
    //    uint32_t acr1;		/* access control register 1 (data) */
    //    uint32_t acr2;		/* access control register 2 (instruction) */
    //    uint32_t acr3;		/* access control register 3 (instruction) */
    //    uint32_t mmubar;		/* MMU base address register */
    //    uint32_t rombar0;		/* ROM base address register 0 */
    //    uint32_t rombar1;		/* ROM base address register 1 */
    //    uint32_t rambar0;		/* RAM base address register 0 */
    //    uint32_t rambar1;		/* RAM base address register 1 */
    //    uint32_t mbar;		/* Module base address register (?) */
  } fix;

  struct {
    uint32_t macsr;		/* EMAC status register */
    uint32_t acc0;		/* accumulator 0 */
    uint32_t acc1;		/* accumulator 1 */
    uint32_t acc2;		/* accumulator 2 */
    uint32_t acc3;		/* accumulator 3 */
    uint32_t accext01;		/* ACC0, ACC1 extensions */
    uint32_t accext23;		/* ACC2, ACC3 extensions */
    uint32_t mask;		/* EMAC mask register */
  } emac;
  /** @brief floating point register save area.
   *
   * This structure describes the layout of the floating point register
   * save area. This is <em>not</em> the layout that is used for the
   * corresponding get/set registers routines.
   */
  struct {
    uint32_t   f_fpcr;		/* floating point control register */
    uint32_t   f_fpsr;		/* floating point status register */
    uint32_t   f_fpiar;		/* floating point instruction address reg */
    uint64_t   f_fp0;
    uint64_t   f_fp1;
    uint64_t   f_fp2;
    uint64_t   f_fp3;
    uint64_t   f_fp4;
    uint64_t   f_fp5;
    uint64_t   f_fp6;
    uint64_t   f_fp7;
  } fpu;
} coldfire_UPCB;

#if (COYOTOS_TARGET == COYOTOS_TARGET_coldfire)

typedef coldfire_UPCB UPCB;

#endif

#endif /* __COLDFIRE_UPCB_H__ */
