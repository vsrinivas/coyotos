#ifndef COLDFIRE_REGISTERS_H
#define COLDFIRE_REGISTERS_H
/*
 * Copyright (C) 2007, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Register structure type definitions.
 *
 * This is moderately awkward, because we really want IDL to own the
 * definitions, but IDL isn't all that gracious about it...
 */

/** @brief Architecture-specific fixed-point registers layout.
 *
 * On the m68k family, including Coldfire, A7 is a shadowed register
 * containing ESP. When executing in user mode, it contains the
 * user-mode stack pointer (USP). When executing in supervisor mode,
 * it contains the supervisor mode stack pointer, and the user-mode
 * stack pointer is accessible through a supervisor-mode
 * instruction. The value stored in the process state is always the
 * <em>user mode</em> stack pointer.
 */
typedef struct coldfire_fixregs_t {
  uint32_t d[8];		/* data registers */
  uint32_t a[8];		/* address registers. A7 is ESP */

  uint32_t pc;
  uint8_t  ccr;			/* least 8 bits of status register */
} coldfire_fixregs_t;

/** @brief Architecture-specific floating point registers layout.
 */ 
typedef struct coldfire_floatregs_t {
  uint32_t        fpcr;		/* floating point control register */
  uint32_t        fpsr;		/* floating point status register */
  uint32_t        fpiar;	/* FP instruction address register */
  uint64_t        fp[8];	/* floating point value registers */
} coldfire_floatregs_t;

/** @brief Extended multiply-accumulate unit registers. */ 
typedef struct coldfire_emacregs_t {
  uint32_t        macsr;	/* MAC status register */
  uint32_t        acc[4];	/* Accumulators acc[1-3] only with
				 * EMAC unit */
  uint32_t        accExt01;	/* ACC0 and ACC1 extensions */
  uint32_t        accExt23;	/* ACC2 and ACC3 extensions */
  uint32_t        mask;         /* MAC mask register */
} coldfire_emacregs_t;

typedef struct coldfire_regset_t {
  coldfire_fixregs_t   fix;
  coldfire_floatregs_t fp;
} coldfire_regset_t;

#if (COYOTOS_ARCH == COYOTOS_ARCH_coldfire)

typedef coldfire_regset_t  regset_t;
typedef coldfire_fixregs_t fixregs_t;

#ifdef __KERNEL__
#define FIX_PC(fix)     (fix.pc)
#define FIX_SP(fix)     (fix.a[7])
#define FIX_FLAGS(fix)  (fix.ccr)

/* Mask out all non-user-mode flags bits. */
   
#define USER_FLAG_MASK  ~0x0000ffu
#endif /* __KERNEL__ */

#endif /* COYOTOS_ARCH == COYOTOS_ARCH_COLDFIRE */

#endif /* COLDFIRE_REGISTERS_H */
