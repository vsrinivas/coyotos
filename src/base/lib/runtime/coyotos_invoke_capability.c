/*
 * Copyright (C) 2007, The EROS Group, LLC.
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

#include <coyotos/syscall.h>

bool invoke_capability(InvParameterBlock_t *pb)
{
  //  register InvParameterBlock_t *pb asm("cx") = ipb;

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
