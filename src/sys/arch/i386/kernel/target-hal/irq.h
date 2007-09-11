#ifndef I386_HAL_IRQ_H
#define I386_HAL_IRQ_H
/*
 * Copyright (C) 2005, The EROS Group, LLC.
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

#include <stdbool.h>

/** @brief Number of distinct interrupt lines.
 *
 * In APIC-based designs, the maximum hypothetical number of available
 * interrupt vectors at the APIC hardware is 256. Of these, NUM_TRAP
 * (32) are used for hardware trap vectors. We do not assign hardware
 * interrupt numbers to those because it is not possible to
 * differentiate them in software. This leaves 224 entries for
 * hardware interrupts. If you find a machine with a need for that
 * many interrupt lines on a single CPU, call us.
 *
 * Note that 224 corresponds conveniently to the Linux NR_IRQ value.
 *
 * Some of the "interrupt" lines are in fact used for system call
 * entry points. Notably, the $0x80 interrupt value (which is vector
 * 96) is used for a legacy system call entry point, and $0x30 is used
 * for the new system call entry point.
 */
/** @brief Number of hardware-defined interrupt entries */
#define TARGET_HAL_NUM_IRQ         224

/** @brief Number of hardware-defined trap entries */
#define TARGET_HAL_NUM_TRAP        32

//#define TARGET_HAL_NUM_VECTOR      (NUM_TRAP+NUM_SYSCALL+NUM_IRQ)


#include "../IA32/EFLAGS.h"

#ifndef __ASSEMBLER__

/** @brief Opaque flags type definition. 
 *
 * Target-specific HAL must define TARGET_FLAGS_T
*/
typedef uint32_t TARGET_FLAGS_T;


static inline TARGET_FLAGS_T locally_disable_interrupts()
{
  uint32_t x;
  __asm__ __volatile__("pushfl;cli;popl %0" 
		       : "=g" (x) 
		       : /* no inputs */);
  return x;
}

static inline void locally_enable_interrupts(TARGET_FLAGS_T oldFlags)
{
  __asm__ __volatile__("pushl %0;popfl"
		       : /* no outputs */ 
		       : "g" (oldFlags) 
		       : "memory", "cc");
}

static inline bool local_interrupts_enabled()
{
  uint32_t x;
  __asm__ __volatile__("pushfl;popl %0" 
		       : "=g" (x) 
		       : /* no inputs */);
  return (x & EFLAGS_IF) ? true : false;
}

#endif /* __ASSEMBLER__ */

#endif /* I386_HAL_IRQ_H */
