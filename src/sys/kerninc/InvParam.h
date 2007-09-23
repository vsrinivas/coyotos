#ifndef __KERNINC_INVPARAM_H__
#define __KERNINC_INVPARAM_H__
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
 * @brief Internal structure for expanded invocation parameter block. */

#include <kerninc/capability.h>
#include <hal/syscall.h>

/** @brief Source capabilities for an invocation.
 *
 * These represent the incoming arguments. The @p cap field will point
 * to the capability. @p theCap will be used only for memory
 * arguments, because we may need to weaken those.
 */
typedef struct SrcCap {
  capability *cap;		/* pointer to source cap. */
  capability theCap;		/* Copy iff sourced from memory. Need a
				   copy to deal with RESTR_WK cases. */
} SrcCap;

typedef struct DestCap {
  capability *cap;		/* pointer to dest cap */
  capability *capPg;		/* pointer to containing CapPag iff TRANSMAPPED */
} DestCap;

typedef struct InvParam {
  struct Process *invoker;
  struct Process *invokee;

  struct Endpoint *invokeeEP;

  struct SrcCap  iCap;
  struct SrcCap  srcCap[4];
  struct DestCap destCap[4];

  uint32_t   sndLen;

  /* Following are only referenced in the kernel capability
   * implementations. Note that THE opw FIELDS ARE NOT INITIALIZED IF
   * WE ARE PERFORMING AN IPC.
   *
   * In BitC we would need to handle this with a separate structure
   * and pass an extra parameter to the handler procedures. */
  size_t     next_idw;
  size_t     next_odw;

  uintptr_t  ipw0;
  uintptr_t  opCode;

  uintptr_t  opw[8];
} InvParam_t;


static inline uint8_t
get_iparam8(InvParam_t* iParam)
{
  if (IPW0_LDW(iParam->ipw0) < iParam->next_idw)
    return 0;
  iParam->next_idw++;
  return get_pw(iParam->invoker, iParam->next_idw-1);
}

static inline uint16_t
get_iparam16(InvParam_t* iParam)
{
  if (IPW0_LDW(iParam->ipw0) < iParam->next_idw)
    return 0;
  iParam->next_idw++;
  return get_pw(iParam->invoker, iParam->next_idw-1);
}

static inline uint32_t
get_iparam32(InvParam_t* iParam)
{
  if (IPW0_LDW(iParam->ipw0) < iParam->next_idw)
    return 0;
  iParam->next_idw++;
  return get_pw(iParam->invoker, iParam->next_idw-1);
}

/* This is the fun bit. On 32-bit platforms, a 64-bit scalar that is
 * pased in registers is passed in an even-numbered parameter word pair.
 * Regardless of native calling convention, the lower-numbered
 * parameter word contains the portion of the value at the least
 * significant address.
 *
 * If this implementation changes, be sure to update the rewriting
 * logic in the kernel sleep capability implementation. */
static inline uint64_t
get_iparam64(InvParam_t* iParam)
{
  if (sizeof(uintptr_t) == 8) {
    if (IPW0_LDW(iParam->ipw0) < iParam->next_idw)
      return 0;
    iParam->next_idw++;
    return get_pw(iParam->invoker, iParam->next_idw-1);
  }
  else if (sizeof(uintptr_t) == 4) {
    if ( (alignof(uintptr_t) == 8) && ((iParam->next_idw % 2) != 0) )
      iParam->next_idw++;

    iParam->next_idw += 2;

    if (IPW0_LDW(iParam->ipw0) < iParam->next_idw-1)
      return 0;

    uint64_t ull;
    ((uint32_t *) &ull)[0] = get_pw(iParam->invoker, iParam->next_idw-2);
    ((uint32_t *) &ull)[1] = get_pw(iParam->invoker, iParam->next_idw-1);

    return ull;
  }
}

static inline void
put_oparam8(InvParam_t* iParam, uint8_t v)
{
  iParam->opw[iParam->next_odw++] = v;
}

static inline void
put_oparam16(InvParam_t* iParam, uint16_t v)
{
  iParam->opw[iParam->next_odw++] = v;
}

static inline void
put_oparam32(InvParam_t* iParam, uint32_t v)
{
  iParam->opw[iParam->next_odw++] = v;
}

static inline void
put_oparam64(InvParam_t* iParam, uint64_t v)
{
  if (sizeof(uintptr_t) == 8) {
    iParam->opw[iParam->next_odw++] = v;
  }
  else if (sizeof(uintptr_t) == 4) {
    if ( (alignof(uintptr_t) == 8) && ((iParam->next_odw % 2) == 1) )
      iParam->next_odw++;

    iParam->opw[iParam->next_odw++] = ((uint32_t *) &v)[0];
    iParam->opw[iParam->next_odw++] = ((uint32_t *) &v)[1];
  }
}


static inline bool
InvTestArguments(InvParam_t *iParam, size_t last_cap, uint32_t sz, uint32_t max)
{
  uintptr_t icw = get_icw(iParam->invoker);
  uintptr_t icw_args = 
    IPW0_MAKE_LDW(iParam->next_idw-1) | IPW0_SC | IPW0_MAKE_LSC(last_cap);

  if (iParam->sndLen < sz || iParam->sndLen > max)
    return false;

  return ((icw & (IPW0_LDW_MASK|IPW0_LSC_MASK|IPW0_SC|IPW0_EX)) == icw_args);
}

static inline uintptr_t InvResult(InvParam_t *iParam, size_t nCap)
{
  uintptr_t opw0 = IPW0_MAKE_LDW(iParam->next_odw-1);
  if (nCap)
    opw0 |= (IPW0_SC | IPW0_MAKE_LSC(nCap-1));
  return opw0;
}

static inline void InvErrorMessage(InvParam_t *iParam, uint64_t rc)
{
  put_oparam64(iParam, rc);
  iParam->opw[0] = InvResult(iParam, 0) | IPW0_EX;
}

static inline void InvTypeMessage(InvParam_t *iParam, uint64_t kt)
{
  put_oparam64(iParam, kt);
  iParam->opw[0] = InvResult(iParam, 0);
}

// This one will accept a min-max bounded string
#define INV_REQUIRE_ARGS_S_M(iParam, lcap, min, max)	    \
  do {							    \
    if (!InvTestArguments(iParam, lcap, min, max)) {	    \
      sched_commit_point();				    \
      InvErrorMessage(iParam, RC_coyotos_Cap_RequestError); \
      return;						    \
    }							    \
  } while (0)

// This one will accept an exactly-sized string
#define INV_REQUIRE_ARGS_S(iParam, lcap, sz)		    \
  INV_REQUIRE_ARGS_S_M(iParam, lcap, sz, sz)

// This one will not
#define INV_REQUIRE_ARGS(iParam, lcap)			    \
   INV_REQUIRE_ARGS_S(iParam, lcap, 0)

#endif /* __KERNINC_INVPARAM_H__ */
