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
 * @brief Elf Process Handler
 */

/* Based on template for processing the following interfaces:
    coyotos.MemoryHandler
    coyotos.ElfProcessHandler
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

#include "coyotos.ElfProcessHandler.h"
#include "coyotos.TargetInfo.h"

/* all of our handler procedures are static */
#define IDL_SERVER_HANDLER_PREDECL static

#include <idl/coyotos/ElfProcessHandler.server.h>
#include <idl/coyotos/MemoryHandler.server.h>

#include "elf.h"

typedef union {
  _IDL_IFUNION_coyotos_ElfProcessHandler
      coyotos_ElfProcessHandler;
  _IDL_IFUNION_coyotos_MemoryHandler
      coyotos_MemoryHandler;
  InvParameterBlock_t pb;
  InvExceptionParameterBlock_t except;
  uintptr_t icw;
} _IDL_GRAND_SERVER_UNION;

#define CR_ELFFILE	 coyotos_ElfProcessHandler_APP_ELFFILE
#define CR_ADDRSPACE	 coyotos_ElfProcessHandler_APP_ADDRSPACE
#define CR_OPAQUESPACE	 coyotos_ElfProcessHandler_APP_OPAQUESPACE
#define CR_SPACEGPT	 coyotos_ElfProcessHandler_APP_SPACEGPT
#define CR_BGGPT	 coyotos_ElfProcessHandler_APP_BGGPT
#define CR_HANDLER_ENTRY coyotos_ElfProcessHandler_APP_HANDLER_ENTRY

#define CR_TMP1		 coyotos_ElfProcessHandler_APP_TMP1
#define CR_TMP2		 coyotos_ElfProcessHandler_APP_TMP2
#define CR_TMP3		 coyotos_ElfProcessHandler_APP_TMP3

typedef struct IDL_SERVER_Environment {
  bool isEPH;
} ISE;

IDL_Environment _IDL_E = {
  .epID = 0,
  .replyCap = CR_REPLYEPT,
};

IDL_Environment * const IDL_E = &_IDL_E;

static uint64_t
HANDLE_coyotos_Cap_destroy(ISE *ise)
{
  /* we are always destroyed along with our process */
  return (RC_coyotos_Cap_NoAccess);
}

static uint64_t
HANDLE_coyotos_Cap_getType(uint64_t *out, ISE *_env)
{
  if (_env->isEPH)
    *out = IKT_coyotos_ElfProcessHandler;
  else
    *out = IKT_coyotos_MemoryHandler;
  return RC_coyotos_Cap_OK;
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
  caploc_t cap = CR_TMP1;
  caploc_t next = CR_TMP2;
  caploc_t spare = CR_TMP3;
  caploc_t next_spare = CR_TMP1;

  /*
   * We will loop as we go down;  the capability registers used looks like:
   *      cap        next     spare    next_spare
   *  1.  TMP1       TMP2     TMP3     TMP1
   *  2.  TMP2       TMP3     TMP1     TMP2
   *  3.  TMP3       TMP1     TMP2     TMP3
   *  4.  TMP1       TMP2     TMP3     TMP1
   *  ... etc ...
   */

  coyotos_Memory_l2value_t l2v = 0;
  coyotos_Memory_l2value_t unusedl2v = 0;
  coyotos_Memory_l2value_t lastl2v = COYOTOS_SOFTADDR_BITS;

  for (;;) {
    if (!coyotos_GPT_getl2v(cap, &l2v, IDL_E))
      return false;

    uintptr_t slot = highbits_shifted(addr, l2v);
    uintptr_t remaddr = lowbits(addr, l2v);

    /// @bug at some point, we need to deal with too-low l2v values.
    /// For now, we'll just fail.
    if (!coyotos_AddressSpace_getSlot(cap, slot, next, IDL_E))
      return false;

    guard_t theGuard;
    bool invalidCap = false;

    // for invalid capabilities, treat them as if they had no guard.
    //
    // This simplifies the code below, at the expense of adding
    // unnecessary GPTs along the way, if remaddr is non-zero.
    if (!coyotos_Memory_getGuard(next, &theGuard, IDL_E)) {
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
				   CR_NULL,
				   IDL_E)) {
	return false;
      }

      // If the cap was invalid (i.e. Null), don't install anything in
      // the chosen slot.
      if (!coyotos_GPT_setl2v(spare, new_l2v, &unusedl2v, IDL_E) ||
	  !coyotos_AddressSpace_guardedSetSlot(cap,
					       slot,
					       spare,
					       make_guard(new_match, new_l2g),
					       IDL_E) ||
	  (!invalidCap && 
	   !coyotos_AddressSpace_guardedSetSlot(spare,
						the_slot,
						next,
						make_guard(the_match, l2g),
						IDL_E))) {
	(void) coyotos_SpaceBank_free(CR_SPACEBANK, 1, spare, CR_NULL, CR_NULL,
				      IDL_E);
	return false;
      }
      continue;  // re-execute loop with newly inserted GPT
    }
    
    coyotos_Memory_restrictions restr = 0;

    if (invalidCap)
      restr = coyotos_Memory_restrictions_readOnly;
    else if (!coyotos_Memory_getRestrictions(next, &restr, IDL_E)) {
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
      if (!coyotos_Cap_getType(next, &type, IDL_E)) {
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
				   CR_NULL,
				   IDL_E))
	return false;

      // copy the existing data, reduce the cap appropriately, and install it.
      if (invalidCap) {
	if (!coyotos_AddressSpace_setSlot(cap, slot, spare, IDL_E)) {
	  (void) coyotos_SpaceBank_free(CR_SPACEBANK, 1, 
					spare, CR_NULL, CR_NULL,
					IDL_E);
	  return false;
	}
      } else if (!coyotos_AddressSpace_copyFrom(spare, next, spare, IDL_E) ||
		 !coyotos_Memory_reduce(spare, new_restr, spare, IDL_E) ||
		 !coyotos_AddressSpace_setSlot(cap, slot, spare, IDL_E)) {
	(void) coyotos_SpaceBank_free(CR_SPACEBANK, 1, 
				      spare, CR_NULL, CR_NULL,
				      IDL_E);
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

typedef struct region {
  enum {
    LOAD,
    CAP,
    STACK
  } type;
  uint32_t perms;
  uint64_t vaddr;
  uint64_t memsz;
  uint64_t foffset;
  uint64_t filesz;
} region;

region capRegion = { CAP, PF_R | PF_W, 0x0, COYOTOS_PAGE_SIZE, 0, 0 };
region stackRegion =  { STACK, PF_R | PF_W };

region textRegion;
region dataRegion;

static bool
find_phdrs(const char *base)
{
  const Elf32_Ehdr *ehdr = (void *)base;

  const char *PhdrBase = (base + ehdr->e_phoff);

  bool foundText = false;
  bool foundData = false;

  int idx;
  for (idx = 0; idx < ehdr->e_phnum; idx++) {
    Elf32_Phdr *phdr = (Elf32_Phdr *)(PhdrBase + idx * ehdr->e_phentsize);

    if (phdr->p_type == PT_GNU_STACK) {
      stackRegion.perms = PF_R | PF_W | (phdr->p_flags & PF_X);
      continue;
    }
    if (phdr->p_type != PT_LOAD)
      continue;

    region *nReg = 0;
    switch (phdr->p_flags & (PF_R | PF_W | PF_X)) {
    case (PF_R|PF_X):
      if (foundText)
	return false;
      foundText = true;
      nReg = &textRegion;
      break;

    case (PF_R|PF_W):
      if (foundData)
	return false;
      foundData = true;
      nReg = &dataRegion;
      break;

    default:
      return false;
    }
    nReg->type = LOAD;
    nReg->perms = (phdr->p_flags & (PF_R | PF_W | PF_X));
    nReg->vaddr = phdr->p_vaddr;
    nReg->memsz = phdr->p_memsz;
    nReg->foffset = phdr->p_offset;
    nReg->filesz = phdr->p_filesz;
  }

  /* set up a maximal stack region for now */
  stackRegion.vaddr = COYOTOS_PAGE_SIZE;
  stackRegion.memsz = 
    coyotos_TargetInfo_large_stack_pointer - COYOTOS_PAGE_SIZE;

  return true;
}

static bool
in_region(region *r, uint64_t addr)
{
  return (addr - r->vaddr >= r->memsz);
}

static uint64_t
HANDLE_coyotos_ElfProcessHandler_setBreak(uint64_t newBreak, ISE *_env)
{
  // right now, we only allow growing the break
  if (newBreak < (dataRegion.vaddr + dataRegion.filesz) ||
      newBreak < (dataRegion.vaddr + dataRegion.memsz))
    return RC_coyotos_Cap_RequestError;

  dataRegion.memsz = (newBreak - dataRegion.vaddr);
  return RC_coyotos_Cap_OK;
}

static uint64_t
HANDLE_coyotos_SpaceHandler_getSpace(caploc_t _retVal, ISE *_env)
{
  cap_copy(_retVal, CR_OPAQUESPACE);

  return (RC_coyotos_Cap_OK);
}

static void
HANDLE_coyotos_MemoryHandler_handle(caploc_t proc,
				    coyotos_Process_FC faultCode,
				    uint64_t faultInfo,
				    ISE *_env)
{
  bool handled = false;

  // get the address space GPT in place
  cap_copy(CR_TMP1, CR_SPACEGPT);

  switch (faultCode) {
  case coyotos_Process_FC_InvalidDataReference:
  case coyotos_Process_FC_AccessViolation:
    if (in_region(&stackRegion, faultInfo) ||
	in_region(&dataRegion, faultInfo))
      handled = process_fault(faultInfo, false);
    break;

  case coyotos_Process_FC_InvalidCapReference:
    if (!in_region(&capRegion, faultInfo))
      break;
    handled = process_fault(faultInfo, true);
    break;

  default:
    break;
  }

  // resume the process, clearing the fault if we handled it
  coyotos_Process_resume(proc, handled, IDL_E);
}
				    
/* You should supply a function that selects an interface
 * type based on the incoming endpoint ID and protected
 * payload */
static inline uint64_t 
choose_if(uint64_t epID, uint32_t pp)
{
  switch (pp) {
  case coyotos_ElfProcessHandler_PP_EPH:
    return IKT_coyotos_ElfProcessHandler;

  case coyotos_ElfProcessHandler_PP_Handler:
    return IKT_coyotos_MemoryHandler;

  default:
    return IKT_coyotos_Cap;
  }
} 

bool
initialize(void)
{
  guard_t theGuard = make_guard(0, COYOTOS_HW_ADDRESS_BITS);
  coyotos_Memory_l2value_t l2v = COYOTOS_HW_ADDRESS_BITS - coyotos_GPT_l2slots;
  coyotos_Memory_l2value_t unusedl2v = 0;

  if (!coyotos_AddressSpace_getSlot(CR_TOOLS, 
				    coyotos_ElfProcessHandler_TOOL_ELFFILE,
				    CR_ELFFILE,
				    IDL_E))
    goto fail;

  if (!coyotos_SpaceBank_alloc(CR_SPACEBANK,
			       coyotos_Range_obType_otGPT,
			       coyotos_Range_obType_otInvalid,
			       coyotos_Range_obType_otInvalid,
			       CR_ADDRSPACE,
			       CR_NULL,
			       CR_NULL,
			       IDL_E))
    goto fail;

  if (!coyotos_Memory_setGuard(CR_ADDRSPACE, theGuard, CR_ADDRSPACE, IDL_E) ||
      !coyotos_GPT_setl2v(CR_ADDRSPACE, l2v, &unusedl2v, IDL_E))
    goto fail;

  if (!coyotos_Process_getSlot(CR_SELF, 
			       coyotos_Process_cslot_addrSpace,
			       CR_TMP1, IDL_E))
    goto fail;

  if (!coyotos_AddressSpace_setSlot(CR_ADDRSPACE, 0, CR_TMP1, IDL_E))
    goto fail;

  if (!coyotos_Process_setSlot(CR_SELF, 
			       coyotos_Process_cslot_addrSpace,
			       CR_ADDRSPACE, IDL_E))
    goto fail;

  // We're now running with the full address space available.  Install our
  // file so that we can read it.
  if (!coyotos_AddressSpace_setSlot(CR_ADDRSPACE, 1, CR_ELFFILE, IDL_E))
    goto fail;

  // process its elf sections
  if (!find_phdrs((char *)(1ul << l2v)))
    goto fail;

  // and unmap it now that we're through.
  if (!coyotos_AddressSpace_setSlot(CR_ADDRSPACE, 1, CR_NULL, IDL_E))
    goto fail;

  // now, set up our capabilities and return our entry cap.
  if (!coyotos_AddressSpace_getSlot(CR_TOOLS, 
				    coyotos_ElfProcessHandler_TOOL_BACKGROUND,
				    CR_BGGPT,
				    IDL_E))
    goto fail;

  if (!coyotos_Memory_reduce(CR_BGGPT,
			     coyotos_Memory_restrictions_weak,
			     CR_BGGPT,
			     IDL_E))
    goto fail;

  coyotos_Memory_l2value_t old_l2v;

  if (!coyotos_SpaceBank_alloc(CR_SPACEBANK,
			       coyotos_Range_obType_otGPT,
			       coyotos_Range_obType_otInvalid,
			       coyotos_Range_obType_otInvalid,
			       CR_SPACEGPT,
			       CR_NULL,
			       CR_NULL,
			       IDL_E) ||
      !coyotos_Endpoint_makeEntryCap(CR_INITEPT, 
				     coyotos_ElfProcessHandler_PP_Handler, 
				     CR_HANDLER_ENTRY, 
				     IDL_E) ||
      !coyotos_Memory_setGuard(CR_SPACEGPT,
			       make_guard(0, COYOTOS_SOFTADDR_BITS),
			       CR_SPACEGPT,
			       IDL_E) ||
      !coyotos_GPT_setl2v(CR_SPACEGPT,
			  COYOTOS_SOFTADDR_BITS - 1, 
			  &old_l2v,
			  IDL_E) ||
      !coyotos_AddressSpace_setSlot(CR_SPACEGPT,
				    coyotos_GPT_handlerSlot,
				    CR_HANDLER_ENTRY,
				    IDL_E) ||
      !coyotos_GPT_setHandler(CR_SPACEGPT, true, IDL_E) ||
      !coyotos_AddressSpace_setSlot(CR_SPACEGPT,
				    0,
				    CR_BGGPT,
				    IDL_E) ||
      !coyotos_Memory_reduce(CR_SPACEGPT,
			     coyotos_Memory_restrictions_opaque,
			     CR_OPAQUESPACE,
			     IDL_E) ||
      !coyotos_Endpoint_makeEntryCap(CR_INITEPT, 
				     coyotos_ElfProcessHandler_PP_EPH, 
				     CR_REPLY0, 
				     IDL_E))
    goto fail;
    
  reply_coyotos_Constructor_create(CR_RETURN, CR_REPLY0);

  return true;

 fail:
  (void) coyotos_SpaceBank_destroyBankAndReturn(CR_SPACEBANK, 
						CR_RETURN,
						IDL_E->errCode, 
						IDL_E);
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
    _env->isEPH = (gsu.pb.u.pp == coyotos_ElfProcessHandler_PP_EPH);

    switch(choose_if(gsu.pb.epID, gsu.pb.u.pp)) {
    case IKT_coyotos_ElfProcessHandler:
      _IDL_IFDISPATCH_coyotos_ElfProcessHandler(&gsu.coyotos_ElfProcessHandler, _env);
      break;
    case IKT_coyotos_MemoryHandler:
      _IDL_IFDISPATCH_coyotos_MemoryHandler(&gsu.coyotos_MemoryHandler, _env);
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
