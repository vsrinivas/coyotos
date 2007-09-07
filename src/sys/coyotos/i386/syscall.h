#ifndef __I386_SYSCALL_H__
#define __I386_SYSCALL_H__
/*
 * Copyright (C) 2007, Jonathan S. Shapiro.
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

#include <stdbool.h>

#if defined(__ASSEMBLER__) && !defined(__KERNEL__)

/** @brief Do a raw syscall using the invocation structure located at
 * @p pws.
 *
 * This is designed to be used by read-only assembly routines, like those
 * in ProtoSpace and the small-space initialization code.  All registers,
 * *including ESP*, are overwritten by this call.  The only state retained
 * is the PC.
 */
#define DO_RO_SYSCALL(pws) \
	movl $pws, %esp		; \
	movl 0(%esp), %eax	; \
	movl 4(%esp), %ebx	; \
	movl 8(%esp), %esi	; \
	movl 12(%esp), %edi	; \
	movl %esp, %ecx		; \
	movl $1f, %edx		; \
1:	int $0x30		; 

/** @brief Do a raw system call, but overwrite the third and fourth 
 * parameters. */
#define DO_RO_SYSCALL_LD64(pws, low, high) \
	movl low, %esi		; \
	movl high, %edi		; \
	movl $pws, %esp		; \
	movl 0(%esp), %eax	; \
	movl 4(%esp), %ebx	; \
	movl %esp, %ecx		; \
	movl $1f, %edx		; \
1:	int $0x30		; 

#endif

#if !defined(__ASSEMBLER__) && !defined(__KERNEL__)

static inline bool invoke_capability(InvParameterBlock_t *ipb)
{
  register InvParameterBlock_t *pb asm("cx") = ipb;

  /* As originally specified, the syscall inline asm eats ALL of the
   * available general-purpose registers. When GCC is optimizing, this
   * is fine, because it is smart enough to spill them. Unfortunately,
   * when GCC is NOT optimizing, it gets pretty cranky about this.
   *
   * In order to help GCC be happy, balanced, and sane, we hand-save
   * and restore %ecx rather than indicating that it is killed by the
   * operation. This leaves /pb/ live in ECX, with the result that GCC
   * is able to figure out what to do even when not optimizing.
   */

  /* Note that the following may clobber values that are unused. This
     is more efficient than testing the number of parameter words
     transmitted/returned. */

  uintptr_t pw0 = pb->pw[0];

  __asm__ __volatile__ (
    "pushl %%ecx\n"
    "movl %%esp,(%[pwblock])\n	"
    "movl $1f,%%edx\n	"
    "1:int $0x30\n"
    "movl (%%esp),%%esp\n"
    "popl %%ecx\n"
    : /* outputs */
      "=m" (*pb),
      [pw0] "=a" (pw0),
      [pw1] "=b" (pb->pw[1]),
      [pw2] "=S" (pb->pw[2]),
      [pw3] "=D" (pb->pw[3])
    : /* inputs */
      [pwblock] "c" (pb), "m" (*pb),
      "[pw0]" (pw0),
      "[pw1]" (pb->pw[1]),
      "[pw2]" (pb->pw[2]),
      "[pw3]" (pb->pw[3])
    : "dx", "memory"
  );

  pb->pw[0] = pw0;

  if (pw0 & IPW0_EX)
    return false;

  return true;
}

static inline void cap_copy(caploc_t dest, caploc_t source)
{
  uintptr_t pw0 = IPW0_MAKE_NR(sc_CopyCap);

  __asm__ __volatile__ (
    "movl %%esp, %%ecx\n "
    "movl $1f,%%edx\n	"
    "1:int $0x30\n"
    : /* outputs */
      [pw0] "=a" (pw0),
      [pw1] "=b" (source.raw),
      [pw2] "=S" (dest.raw)
    : /* inputs */
      "[pw0]" (pw0),
      "[pw1]" (source.raw),
      "[pw2]" (dest.raw)
    : "dx", "cx", "memory"
  );
}

static inline void yield(void)
{
  uintptr_t pw0 = IPW0_MAKE_NR(sc_Yield);

  __asm__ __volatile__ (
    "movl %%esp, %%ecx\n "
    "movl $1f,%%edx\n	"
    "1:int $0x30\n"
    : /* outputs */
      [pw0] "=a" (pw0)
    : /* inputs */
      "[pw0]" (pw0)
    : "dx", "cx", "memory"
  );
}

#endif /* !defined(__ASSEMBLER__) && !defined(__KERNEL__) */


#endif  /* __I386_SYSCALL_H__ */
