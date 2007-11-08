#ifndef COLDFIRE_HAL_SYSCALL_H
#define COLDFIRE_HAL_SYSCALL_H
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
 * @brief Machine-dependent system call support.
 *
 * These are the procedures that know how to get/set the various
 * system call parameters.
 */

#include <kerninc/Process.h>
#include <coyotos/syscall.h>
#include <target-hal/vm.h>

/** @brief Return TRUE if the non-registerized parameter block falls
 * within valid user-mode addresses according to the rules of the
 * current architecture.
 */
static inline bool valid_param_block(Process *p)
{
  /* Maximum stack-based parameter block is 87 bytes unless we are
     using the scatter/gather engine. */
  return (vm_valid_uva(p, p->state.regs.fix.a[0]) &&
	  vm_valid_uva(p, p->state.regs.fix.a[0] + sizeof(InvParameterBlock_t) - 1));
}

/* TEMPORARY: */
#include <kerninc/printf.h>
/** @brief Fetch input parameter word @p pw from the appropriate
 * architecture-dependent location. */
static inline uintptr_t 
get_paramblock_pw(Process *p, size_t pw)
{
  return ((InvParameterBlock_t *) p->state.regs.fix.a[0])->pw[pw];
}

/** @brief Fetch input parameter word @p pw from the appropriate
 * architecture-dependent location. */
static inline uintptr_t 
get_pw(Process *p, size_t pw)
{
  switch (pw) {
  case 0:
    return p->state.regs.fix.d[0];
  case 1:
    return p->state.regs.fix.d[1];
  case 2:
    return p->state.regs.fix.d[2];
  case 3:
    return p->state.regs.fix.d[3];
  case 4:
    return p->state.regs.fix.d[4];
  case 5:
    return p->state.regs.fix.d[5];
  case 6:
    return p->state.regs.fix.d[6];
  case 7:
    return p->state.regs.fix.d[7];
  case IPW_RCVBOUND:
  case IPW_RCVPTR:
  case IPW_RCVCAP+0:
  case IPW_RCVCAP+1:
  case IPW_RCVCAP+2:
  case IPW_RCVCAP+3:
    bug("Attempt to fetch rcv parameter with get_pw()\n");
  default:
    return get_paramblock_pw(p, pw);
  }
}

/** @brief Fetch receive parameter word @p pw from the appropriate
 * architecture-dependent location. */
static inline uintptr_t 
get_rcv_pw(Process *p, size_t pw)
{
  switch (pw) {
  case 0:
    return p->state.regs.fix.d[0];
  case IPW_RCVBOUND:
    return p->state.regs.fix.a[1];
  case IPW_RCVPTR:
    return p->state.regs.fix.a[2];
  case IPW_RCVCAP+0:
  case IPW_RCVCAP+1:
  case IPW_RCVCAP+2:
  case IPW_RCVCAP+3:
    bug("Attempt to fetch rcv cap through get_rcv_pw()\n");
  default:
    bug("Bad fetch of receive paramater word\n");
  }
}

/** @brief Save the necessary architecture-dependent parameter words
 * that are not transferred across the system call boundary in
 * registers.
 */
static inline void
save_soft_parameters(Process *p)
{
}

/** @brief Copy received soft params back to user.
 *
 * This is @em always executed in the current address space.
 */
static inline void
copyout_soft_parameters(Process *p)
{
}

/** @brief Store input parameter word @p i to the appropriate
 * architecture-dependent INPUT location. Used in transition to
 * receive phase. */
static inline void 
set_pw(Process *p, size_t pw, uintptr_t value)
{
  switch (pw) {
  case 0:
    p->state.regs.fix.d[0] = value;
    return;
  case 1:
    p->state.regs.fix.d[1] = value;
    return;
  case 2:
    p->state.regs.fix.d[2] = value;
    return;
  case 3:
    p->state.regs.fix.d[3] = value;
    return;
  case 4:
    p->state.regs.fix.d[4] = value;
    return;
  case 5:
    p->state.regs.fix.d[5] = value;
    return;
  case 6:
    p->state.regs.fix.d[6] = value;
    return;
  case 7:
    p->state.regs.fix.d[7] = value;
    return;
  case OPW_PP:
    p->state.regs.fix.a[1] = value;
    return;
  case OPW_SNDLEN:
    p->state.regs.fix.a[2] = value;
    return;
  default:
    bug("Do not yet know how to store parameter word %d\n", pw);
  }
}

/** @brief Set receiver epID to incoming epID value. */
static inline void 
set_epID(Process *p, uint64_t epID)
{
  p->state.regs.fix.a[3] = epID;
  p->state.regs.fix.a[4] = epID >> 32;
}

/** @brief Fetch receiver epID for matching. */
static inline uint64_t 
get_rcv_epID(Process *p)
{
  uint64_t epID = p->state.regs.fix.a[4];
  epID <<= 32;
  epID |=  p->state.regs.fix.a[3];
  return epID;
}

static inline uintptr_t
get_icw(Process *p)
{
  return (get_pw(p, 0));
}

static inline void
set_icw(Process *p, uintptr_t val)
{
  return (set_pw(p, 0, val));
}

static inline caploc_t
get_invoke_cap(Process *p)
{
  InvParameterBlock_t *pb = (InvParameterBlock_t *) p->state.regs.fix.a[0];

  return pb->u.invCap;
}

static inline caploc_t
get_snd_cap(Process *p, size_t cap)
{
  InvParameterBlock_t *pb = (InvParameterBlock_t *) p->state.regs.fix.a[0];

  return pb->sndCap[cap];
}

static inline caploc_t
get_rcv_cap(Process *p, size_t cap)
{
  caploc_t cl;
  cl.raw = p->state.regs.fix.a[6];
  return cl;
}
#endif /* COLDFIRE_HAL_SYSCALL_H */
