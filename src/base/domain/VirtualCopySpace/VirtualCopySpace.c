/*
 * Copyright (C) 2007, The EROS Group, LLC
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
 * @brief Virtual Copy Space implementation
 */

/* Based on template for processing the following interfaces:
    coyotos.MemoryHandler
    coyotos.VirtualCopySpace
 */

#include <coyotos/capidl.h>
#include <coyotos/syscall.h>
#include <coyotos/runtime.h>
#include <coyotos/reply_Constructor.h>

#include <string.h>

#include <idl/coyotos/AddressSpace.h>
#include <idl/coyotos/Endpoint.h>
#include <idl/coyotos/GPT.h>
#include <idl/coyotos/Page.h>
#include <idl/coyotos/KernLog.h>
#include <idl/coyotos/CapPage.h>
#include <idl/coyotos/Null.h>
#include <idl/coyotos/Process.h>
#include <idl/coyotos/SpaceBank.h>

#include "coyotos.VirtualCopySpace.h"

/* all of our handler procedures are static */
#define IDL_SERVER_HANDLER_PREDECL static

#include <idl/coyotos/VirtualCopySpace.server.h>
#include <idl/coyotos/MemoryHandler.server.h>
#include <idl/coyotos/SpaceHandler.server.h>

typedef union {
  _IDL_IFUNION_coyotos_VirtualCopySpace
      coyotos_VirtualCopySpace;
  _IDL_IFUNION_coyotos_MemoryHandler
      coyotos_MemoryHandler;
  _IDL_IFUNION_coyotos_SpaceHandler
      coyotos_SpaceHandler;
  InvParameterBlock_t pb;
  InvExceptionParameterBlock_t except;
  uintptr_t icw;
} _IDL_GRAND_SERVER_UNION;

#define CR_SPACEGPT      coyotos_VirtualCopySpace_APP_SPACEGPT
#define CR_OPAQUESPACE   coyotos_VirtualCopySpace_APP_OPAQUESPACE
#define CR_BGGPT         coyotos_VirtualCopySpace_APP_BGGPT
#define CR_HANDLER_ENTRY coyotos_VirtualCopySpace_APP_HANDLER_ENTRY

#define CR_TMP1		 coyotos_VirtualCopySpace_APP_TMP1
#define CR_TMP2		 coyotos_VirtualCopySpace_APP_TMP2
#define CR_TMP3		 coyotos_VirtualCopySpace_APP_TMP3

typedef struct IDL_SERVER_Environment {
  bool isHandlerFacet;
  bool isVCSFacet;
} ISE;

bool frozen = false;

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Cap_destroy(ISE *ise)
{
  /* Only builder caps can destroy the constructor. */
  if (!ise->isVCSFacet)
    return (RC_coyotos_Cap_NoAccess);

  if (!coyotos_SpaceBank_destroyBankAndReturn(CR_SPACEBANK,
					      CR_RETURN,
					      RC_coyotos_Cap_OK)) {
    return (IDL_exceptCode);
  }

  /* Not reached */
  return (RC_coyotos_Cap_RequestError);
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Cap_getType(uint64_t *out, ISE *_env)
{
  if (_env->isVCSFacet)
    *out = IKT_coyotos_VirtualCopySpace;
  else
    *out = IKT_coyotos_MemoryHandler;

  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_VirtualCopySpace_freeze(caploc_t _retVal, ISE *_env)
{
  return RC_coyotos_Cap_UnknownRequest;
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_SpaceHandler_getSpace(caploc_t _retVal, ISE *_env)
{
  cap_copy(_retVal, CR_OPAQUESPACE);

  return (RC_coyotos_Cap_OK);
}

/**
 * @brief Compute min(a, b) in a type-safe, single-evaluation form.  
 *
 * This must be a macro because it's type-indifferent.
 */
#define min(a, b) \
  ({ \
    __typeof(a) tmpa = (a); \
    __typeof(b) tmpb = (b); \
    (((tmpa) < (tmpb)) ? (tmpa) : (tmpb)); \
  })

/**
 * @brief Compute min(a, b) in a type-safe, single-evaluation form.  
 *
 * This must be a macro because it's type-indifferent.
 */
#define max(a, b) \
  ({ \
    __typeof(a) tmpa = (a); \
    __typeof(b) tmpb = (b); \
    (((tmpa) > (tmpb)) ? (tmpa) : (tmpb)); \
  })

static inline uintptr_t highbits_shifted(uint64_t addr, uint8_t shift)
{
  if (shift >= 64)
    return 0;
  return (addr >> shift);
}

static inline uintptr_t highbits(uint64_t addr, uint8_t shift)
{
  if (shift >= 64)
    return 0;
  return addr & ~(((uint64_t)1 << shift) - 1);
}

static inline uint64_t lowbits(uint64_t addr, uint8_t shift)
{
  if (shift >= 64)
    return addr;
  return addr & (((uint64_t)1 << shift) - 1);
}

static inline coyotos_Memory_l2value_t l2offset(uint64_t addr)
{
  coyotos_Memory_l2value_t sz = COYOTOS_PAGE_ADDR_BITS;
  while (sz < 64 && ((uint64_t)1 << sz) <= addr)
    sz++;
  return sz;
}

/// standard integer division:  round up @p a to a multiple of @p b
static inline uint64_t
round_up(uint64_t a, uint64_t b)
{
  return ((a + b - 1)/b) * b;
}

/// @bug current limitations:
///   @li  doesn't fill in read-only faults with a fixed zero page
bool
process_fault(uint64_t addr, bool wantCap)
{
  caploc_t cap = CR_SPACEGPT;
  caploc_t next = CR_TMP1;
  caploc_t spare = CR_TMP2;
  caploc_t next_spare = CR_TMP3;

  /*
   * We will loop as we go down;  the capability registers used looks like:
   *      cap        next     spare    next_spare
   *  1.  SPACEGPT   TMP1     TMP2     TMP3
   *  2.  TMP1       TMP2     TMP3     TMP1
   *  3.  TMP2       TMP3     TMP1     TMP2
   *  4.  TMP3       TMP1     TMP2     TMP3
   *  5.  TMP1       TMP2     TMP3     TMP1
   *  ... etc ...
   */

  coyotos_Memory_l2value_t l2v = 0;
  coyotos_Memory_l2value_t unusedl2v = 0;
  coyotos_Memory_l2value_t lastl2v = COYOTOS_SOFTADDR_BITS;

  for (;;) {
    if (!coyotos_GPT_getl2v(cap, &l2v))
      return false;

    uintptr_t slot = highbits_shifted(addr, l2v);
    uintptr_t remaddr = lowbits(addr, l2v);

    /// @bug at some point, we need to deal with too-low l2v values.
    /// For now, we'll just fail.
    if (!coyotos_AddressSpace_getSlot(cap, slot, next))
      return false;

    guard_t theGuard;
    bool invalidCap = false;

    // for invalid capabilities, treat them as if they had no guard.
    //
    // This simplifies the code below, at the expense of adding
    // unnecessary GPTs along the way, if remaddr is non-zero.
    if (!coyotos_Memory_getGuard(next, &theGuard)) {
      theGuard = make_guard(0, COYOTOS_PAGE_ADDR_BITS);
      invalidCap = true;
    }
    
    uint64_t matchValue = guard_matchValue(theGuard);
    uint64_t mask = guard_mask(theGuard);
    coyotos_Memory_l2value_t l2g = guard_l2g(theGuard);
    
    /* check to see that the address space is well-formed */
    if (l2g > COYOTOS_HW_ADDRESS_BITS || l2g > l2v ||
	highbits_shifted(matchValue, l2v) != 0) {
      return (false);
    }
    
    uint64_t mismatch = ((remaddr ^ matchValue) & mask);
    
    if (mismatch != 0) {
      // we need to add a GPT

      // figure out its guard and l2v
      size_t desired_l2g = 
	min(l2g + round_up(l2offset(mismatch) - l2g, coyotos_GPT_l2slots),
	    COYOTOS_SOFTADDR_BITS);

      size_t new_l2v = max(desired_l2g - coyotos_GPT_l2slots, l2g);
      size_t new_l2g = min(desired_l2g, l2v);

      uint32_t new_match = highbits_shifted(matchValue, new_l2g);

      // figure out the position and new guard for the existing cap
      size_t the_slot = highbits_shifted(lowbits(matchValue, new_l2g),
					 new_l2v);
      uint32_t the_match = highbits_shifted(lowbits(matchValue, new_l2v), l2g);

      // now that that's all figured out, allocate the new cap, and
      // set everything up.
      if (!coyotos_SpaceBank_alloc(CR_SPACEBANK,
				   coyotos_Range_obType_otGPT, 
				   coyotos_Range_obType_otInvalid,
				   coyotos_Range_obType_otInvalid,
				   spare,
				   CR_NULL,
				   CR_NULL)) {
	return false;
      }

      // If the cap was invalid (i.e. Null), don't install anything in
      // the chosen slot.
      if (!coyotos_GPT_setl2v(spare, new_l2v, &unusedl2v) ||
	  !coyotos_AddressSpace_guardedSetSlot(cap,
					       slot,
					       spare,
					       make_guard(new_match, 
							  new_l2g)) ||
	  (!invalidCap && 
	   !coyotos_AddressSpace_guardedSetSlot(spare,
						the_slot,
						next,
						make_guard(the_match, 
							   l2g)))) {
	(void) coyotos_SpaceBank_free(CR_SPACEBANK, 1,
				      spare, CR_NULL, CR_NULL);
	return false;
      }
      continue;  // re-execute loop with newly inserted GPT
    }
    
    coyotos_Memory_restrictions restr = 0;

    if (invalidCap)
      restr = coyotos_Memory_restrictions_readOnly;
    else if (!coyotos_Memory_getRestrictions(next, &restr)) {
      return false;  // shouldn't happen
    }
    if (restr & coyotos_Memory_restrictions_opaque) {
      return false;  // cannot peer through opacity
    }

    if (restr & (coyotos_Memory_restrictions_readOnly | 
		 coyotos_Memory_restrictions_weak)) {
      
      coyotos_Memory_restrictions new_restr = 
	(restr & ~(coyotos_Memory_restrictions_readOnly | 
		   coyotos_Memory_restrictions_weak |
		   coyotos_Memory_restrictions_opaque));

      // we need to replace this capability with a strong capability.
      coyotos_Cap_AllegedType type = 0;
      if (!coyotos_Cap_getType(next, &type)) {
	return false;
      }

      coyotos_Range_obType obType = coyotos_Range_obType_otInvalid;

      switch (type) {
      case IKT_coyotos_Null:
	obType = wantCap ? 
	  coyotos_Range_obType_otCapPage : coyotos_Range_obType_otPage;
	break;
      case IKT_coyotos_Page:
	obType = coyotos_Range_obType_otPage;
	break;
      case IKT_coyotos_CapPage:
	obType = coyotos_Range_obType_otCapPage;
	break;
      case IKT_coyotos_GPT:
	obType = coyotos_Range_obType_otGPT;
	break;
      default:
	obType = coyotos_Range_obType_otInvalid;
	break;
      }

      if (obType == coyotos_Range_obType_otInvalid)
	return false;

      if (!coyotos_SpaceBank_alloc(CR_SPACEBANK,
				   obType,
				   coyotos_Range_obType_otInvalid,
				   coyotos_Range_obType_otInvalid,
				   spare,
				   CR_NULL,
				   CR_NULL))
	return false;

      // copy the existing data, reduce the cap appropriately, and install it.
      if (invalidCap) {
	if (!coyotos_AddressSpace_setSlot(cap, slot, spare)) {
	  (void) coyotos_SpaceBank_free(CR_SPACEBANK, 1, 
					spare, CR_NULL, CR_NULL);
	  return false;
	}
      } else if (!coyotos_AddressSpace_copyFrom(spare, next, spare) ||
		 !coyotos_Memory_reduce(spare, new_restr, spare) ||
		 !coyotos_AddressSpace_setSlot(cap, slot, spare)) {
	(void) coyotos_SpaceBank_free(CR_SPACEBANK, 1, 
				      spare, CR_NULL, CR_NULL);
	return false;
      }

      // if we've upgraded a page, we're all done.
      if (obType == coyotos_Range_obType_otPage ||
	  obType == coyotos_Range_obType_otCapPage)
	return true;

      continue; // re-process with new cap
    }

    // To prevent infinite recursion, we require that address spaces
    // continually reduce l2v.
    if (l2v >= lastl2v) {
      return false;
    }
    lastl2v = l2v;

    // we're traversing *into* a GPT.  Since the guard is zero, and
    // we match it, we just need to update the address and swap around
    // our caps.
    addr = remaddr;

    cap = next;
    next = spare;
    spare = next_spare;
    next_spare = cap;
  }
}

IDL_SERVER_HANDLER_PREDECL void
HANDLE_coyotos_MemoryHandler_handle(caploc_t proc,
				    coyotos_Process_FC faultCode,
				    uint64_t faultInfo,
				    ISE *_env)
{
  bool clearFault = false;

  switch (faultCode) {
  case coyotos_Process_FC_InvalidDataReference:
  case coyotos_Process_FC_AccessViolation:
    clearFault = process_fault(faultInfo, false);
    break;
  case coyotos_Process_FC_InvalidCapReference:
    clearFault = process_fault(faultInfo, true);
    break;

  default:
    break;
  }

  coyotos_Process_resume(proc, clearFault);
}
				    
/* You should supply a function that selects an interface
 * type based on the incoming endpoint ID and protected
 * payload */
static inline uint64_t 
choose_if(uint64_t epID, uint32_t pp)
{
  switch (pp) {
  case coyotos_VirtualCopySpace_PP_VCS:
    return IKT_coyotos_VirtualCopySpace;

  case coyotos_VirtualCopySpace_PP_Handler:
    return IKT_coyotos_MemoryHandler;

  default:
    return IKT_coyotos_Cap;
  }
} 

bool
initialize(void)
{
  guard_t theGuard = 0;
  bool isInvalid = false;

  if (!coyotos_AddressSpace_getSlot(CR_TOOLS, 
				    coyotos_VirtualCopySpace_TOOLS_BACKGROUND,
				    CR_BGGPT))
    goto fail;

  if (!coyotos_Memory_getGuard(CR_BGGPT, &theGuard)) {
    theGuard = make_guard(0, COYOTOS_PAGE_ADDR_BITS);
    isInvalid = true;
  }

  /* right now, we don't support full-size address spaces */
  if (guard_l2g(theGuard) == COYOTOS_SOFTADDR_BITS)
    goto fail;

  if (!coyotos_Memory_reduce(CR_BGGPT,
			     coyotos_Memory_restrictions_weak,
			     CR_BGGPT))
    goto fail;

  coyotos_Memory_l2value_t old_l2v;

  coyotos_AddressSpace_slot_t slot = 
    (guard_matchValue(theGuard) >> (COYOTOS_SOFTADDR_BITS - 1)) ? 1 : 0;

  if (!coyotos_SpaceBank_alloc(CR_SPACEBANK,
			       coyotos_Range_obType_otGPT,
			       coyotos_Range_obType_otInvalid,
			       coyotos_Range_obType_otInvalid,
			       CR_SPACEGPT,
			       CR_NULL,
			       CR_NULL) ||
      !coyotos_Endpoint_makeEntryCap(CR_INITEPT, 
				     coyotos_VirtualCopySpace_PP_Handler, 
				     CR_HANDLER_ENTRY) ||
      !coyotos_Memory_setGuard(CR_SPACEGPT,
			       make_guard(0, COYOTOS_SOFTADDR_BITS),
			       CR_SPACEGPT) ||
      !coyotos_GPT_setl2v(CR_SPACEGPT,
			  COYOTOS_SOFTADDR_BITS - 1, 
			  &old_l2v) ||
      !coyotos_AddressSpace_setSlot(CR_SPACEGPT,
				    coyotos_GPT_handlerSlot,
				    CR_HANDLER_ENTRY) ||
      !coyotos_GPT_setHandler(CR_SPACEGPT, true) ||
      (!isInvalid &&
       !coyotos_AddressSpace_guardedSetSlot(CR_SPACEGPT,
					    slot,
					    CR_BGGPT,
					    theGuard)) ||
      !coyotos_Memory_reduce(CR_SPACEGPT,
			     coyotos_Memory_restrictions_opaque,
			     CR_OPAQUESPACE) ||
      !coyotos_Endpoint_makeEntryCap(CR_INITEPT, 
				     coyotos_VirtualCopySpace_PP_VCS, 
				     CR_REPLY0))
    goto fail;
    
  reply_coyotos_Constructor_create(CR_RETURN, CR_REPLY0);

  return true;

 fail:
  (void) coyotos_SpaceBank_destroyBankAndReturn(CR_SPACEBANK, 
						CR_RETURN,
						IDL_exceptCode);
  return false;
}

/* The IDL_SERVER_Environment structure type is something
 * that you should define to hold any "extra" information
 * you need to carry around in your handlers. CapIDL code
 * will pass this pointer along, but knows absolutely
 * nothing about the contents of the structure.
 *
 * If you do not need any extra information, you can pass
 * a NULL pointer to ProcessRequests()
 */
void
ProcessRequests(struct IDL_SERVER_Environment *_env)
{
  _IDL_GRAND_SERVER_UNION gsu;
  
  gsu.icw = 0;
  gsu.pb.sndPtr = 0;
  gsu.pb.sndLen = 0;
  
  for(;;) {
    gsu.icw &= (IPW0_LDW_MASK|IPW0_LSC_MASK
        |IPW0_SG|IPW0_SP|IPW0_SC|IPW0_EX);
    gsu.icw |= IPW0_MAKE_NR(sc_InvokeCap)|IPW0_RP|IPW0_AC
        |IPW0_MAKE_LRC(3)|IPW0_NB|IPW0_CO;
    
    gsu.pb.u.invCap = CR_RETURN;
    gsu.pb.rcvCap[0] = CR_RETURN;
    gsu.pb.rcvCap[1] = CR_ARG0;
    gsu.pb.rcvCap[2] = CR_ARG1;
    gsu.pb.rcvCap[3] = CR_ARG2;
    gsu.pb.rcvBound = (sizeof(gsu) - sizeof(gsu.pb));
    gsu.pb.rcvPtr = ((char *)(&gsu)) + sizeof(gsu.pb);
    
    invoke_capability(&gsu.pb);
    
    /* Re-establish defaults. Note we rely on the handler proc
     * to decide how MANY of these caps will be sent by setting ICW.SC
     * and ICW.lsc fields properly.
     */
    gsu.pb.sndCap[0] = CR_REPLY0;
    gsu.pb.sndCap[1] = CR_REPLY1;
    gsu.pb.sndCap[2] = CR_REPLY2;
    gsu.pb.sndCap[3] = CR_REPLY3;
    
    /* We rely on the (de)marshaling procedures to set sndLen to zero
     * if no string is to be sent. We cannot zero it preemptively here
     * because sndLen is an IN parameter telling how many bytes we got.
     * Set sndPtr to zero so that we will fault if this is mishandled.
     */
    gsu.pb.sndPtr = 0;
    
    if ((gsu.icw & IPW0_SC) == 0) {
      /* Protocol violation -- reply slot unpopulated. */
      gsu.icw = 0;
      gsu.pb.sndLen = 0;
      continue;
    }
    
    switch(choose_if(gsu.pb.epID, gsu.pb.u.pp)) {
    case IKT_coyotos_VirtualCopySpace:
      _IDL_IFDISPATCH_coyotos_VirtualCopySpace(&gsu.coyotos_VirtualCopySpace, _env);
      break;
    case IKT_coyotos_MemoryHandler:
      _IDL_IFDISPATCH_coyotos_MemoryHandler(&gsu.coyotos_MemoryHandler, _env);
      break;
    case IKT_coyotos_SpaceHandler:
      _IDL_IFDISPATCH_coyotos_SpaceHandler(&gsu.coyotos_SpaceHandler, _env);
      break;
    default:
      {
        gsu.except.icw =
          IPW0_MAKE_LDW((sizeof(gsu.except)/sizeof(uintptr_t))-1)
          |IPW0_EX|IPW0_SP;
        gsu.except.exceptionCode = RC_coyotos_Cap_UnknownRequest;
        gsu.pb.sndLen = 0;
        break;
      }
    }
  }
}

int
main(int argc, char *argv[])
{
  ISE vcs_ISE = {
  };

  memset(&vcs_ISE, 0, sizeof (vcs_ISE));

  if (!initialize())
    return (0);

  ProcessRequests(&vcs_ISE);

  return 0;
}
