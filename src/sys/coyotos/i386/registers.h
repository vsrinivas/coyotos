#ifndef I386_REGISTERS_H
#define I386_REGISTERS_H
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
typedef struct i386_fixregs_t {
  uint32_t EDI;
  uint32_t ESI;
  uint32_t EBP;
  uint32_t ExceptAddr;
  uint32_t EBX;
  uint32_t EDX;
  uint32_t ECX;
  uint32_t EAX;
  uint32_t ExceptNo;
  uint32_t Error;
  uint32_t EIP;
  uint32_t CS;
  uint32_t EFLAGS;
  uint32_t ESP;
  uint32_t SS;
  uint32_t ES;
  uint32_t DS;
  uint32_t FS;
  uint32_t GS;
} i386_fixregs_t;

typedef uint32_t i386_ssereg_t[4];

/** @brief Architecture-specific floating point and SIMD registers layout.
 *
 * There is a policy problem here: Intel changed the format starting
 * in the Pentium-III (FSAVE -&gt; FXSAVE), and they have reserved
 * space for future extensions. This raises a tricky question about
 * what to present at the interface.
 *
 * It is possible to reconstruct the FSAVE information from the
 * FXSAVE information. It is also possible to run on an older
 * machine by initializing the SSE slots to well-defined values and
 * pretending that since no SSE instructions were executed the state
 * must not have changed. That is what we do here.
 *
 * This leaves us with the question of what type to present at the
 * interface. Our sense is that it is better to present a
 * well-defined, shorter structure than to give a general structure
 * whose internals will need to be edited later. If necessary we can
 * add a new fetch operation to deal with future extensions to SSE
 * state.
 */ 
typedef struct i386_fxsave32_t {
  uint16_t         fcw;
  uint16_t         fsw;
  uint16_t         ftw;
  uint16_t         fop;
  uint32_t         eip;
  uint32_t         cs;
  uint32_t         dp;
  uint32_t         ds;
  uint32_t         mxcsr;
  uint32_t         mxcsr_mask;
  i386_ssereg_t    fpr[8];	// a.k.a. mmx
  i386_ssereg_t    xmm[8];
} i386_fxsave32_t;

typedef union i386_floatregs_t {
  i386_fxsave32_t fxregs;
  uint32_t        fxsave[128] __attribute__ ((aligned (16)));
} i386_floatregs_t;

typedef struct i386_softregs_t {
  /* pw0 via %eax */
  /* pw1 via %ebx */
  /* pw2 via %esi */
  /* pw3 via %edi */
  uint32_t        pw4;		/* OUT - not saved on entry */
  uint32_t        pw5;		/* OUT - not saved on entry */
  uint32_t        pw6;		/* OUT - not saved on entry */
  uint32_t        pw7;		/* OUT - not saved on entry */

  uint32_t        protPayload;  /* OUT - not saved on entry */
  uint32_t        sndLen;	/* OUT - not saved on entry*/

  uint32_t        rcvBound;	/* RCV - saved on entry */
  uint32_t        rcvPtr;	/* RCV - saved on entry */

  caploc32_t      cdest[4];	/* RCV - saved on entry */

  uint64_t        epID;		/* RCV/OUT - saved on entry */
} i386_softregs_t;

typedef struct i386_regset_t {
  i386_fixregs_t   fix;
  i386_floatregs_t fp;
  i386_softregs_t  soft;
} i386_regset_t;

#if (COYOTOS_ARCH == COYOTOS_ARCH_i386)
typedef i386_regset_t  regset_t;
typedef i386_fixregs_t fixregs_t;

#ifdef __KERNEL__
#define FIX_PC(regs)     (regs.fix).EIP
#define FIX_SP(regs)     (regs.fix).ESP
#define FIX_FLAGS(regs)  (regs.fix).EFLAGS

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

#endif /* COYOTOS_ARCH == COYOTOS_ARCH_i386 */

#endif /* I386_REGISTERS_H */
