#ifndef ARM_REGISTERS_H
#define ARM_REGISTERS_H
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

/** @brief Architecture-specific fixed-point registers layout */
typedef struct arm_fixregs_t {
  uint32_t dummy;
} arm_fixregs_t;

/** @brief Architecture-specific floating point and SIMD registers layout.
 */ 

typedef union arm_floatregs_t {
  uint32_t dummys;
} arm_floatregs_t;

#if (COYOTOS_ARCH == COYOTOS_ARCH_arm)
typedef arm_fixregs_t   fixregs_t;
typedef arm_floatregs_t floatregs_t;

#ifdef __KERNEL__
#define FIX_PC(fix)     (fix).pc
#define FIX_SP(fix)     (fix).sp
#define FIX_FLAGS(fix)  (fix).flags

/* To mask out the privileged bits of EFLAGS, we follow the same rules
   as user-mode POPF. The description of POPF reads:

   When operandsize == 32, CPL > IOPL:

     All non-reserved bits except IF, IOPL, VIP, and VIF can be
     modified. IF, IOPL, VM, and all reserved bits are unaffected.
     VIP and VIF are cleared.

   In *all* popf operations, the following happen:

     VIP and VIF are cleared
     VM is unaffected.
     Reserved bits are unaffected.

   Therefore, we only need to mask out: 

   So we need to mask out:
 
      IF       0x00000200    interrupt
      IOPL     0x00003000    io privilege level

   As of 5/5/2007, the reserved bits are:

      reserved 0xFFC0802A    various reserved bits

   We could mask these out, but Intel has been known to extend them in
   backwards compatible ways. For the moment, I have chosen NOT to
   mask them.
 */
   
#define USER_FLAG_MASK  ~0x00003200u
#endif /* __KERNEL__ */

#endif /* COYOTOS_ARCH == COYOTOS_ARCH_arm */

#endif /* ARM_REGISTERS_H */
