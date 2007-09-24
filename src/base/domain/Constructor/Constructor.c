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
 * @brief Constructor implementation
 */

/* Based on template for processing the following interfaces:
    coyotos.Verifier
    coyotos.Constructor
    coyotos.Builder
 */

#include <coyotos/capidl.h>
#include <coyotos/syscall.h>
#include <coyotos/runtime.h>
#include <coyotos/reply_Constructor.h>

#include <idl/coyotos/AddressSpace.h>
#include <idl/coyotos/Discrim.h>
#include <idl/coyotos/Endpoint.h>
#include <idl/coyotos/GPT.h>
#include <idl/coyotos/Process.h>

/* all of our handler procedures are static */
#define IDL_SERVER_HANDLER_PREDECL static

#include <idl/coyotos/Verifier.server.h>
#include <idl/coyotos/Constructor.server.h>
#include <idl/coyotos/Builder.server.h>

#include "coyotos.Constructor.h"

#define CR_NEW_PROC    		coyotos_Constructor_APP_NEW_PROC
#define CR_NEW_ENDPT		coyotos_Constructor_APP_NEW_ENDPT
#define CR_NEW_RENDPT		coyotos_Constructor_APP_NEW_RENDPT
#define CR_VERIFIER		coyotos_Constructor_APP_VERIFIER
#define CR_YIELD_TOOLS		coyotos_Constructor_APP_YIELD_TOOLS
#define CR_YIELD_BRAND		coyotos_Constructor_APP_YIELD_BRAND
#define CR_YIELD_ADDRSPACE	coyotos_Constructor_APP_YIELD_ADDRSPACE
#define CR_YIELD_HANDLER    	coyotos_Constructor_APP_YIELD_HANDLER
#define CR_YIELD_PROTOSPACE	coyotos_Constructor_APP_YIELD_PROTOSPACE
#define CR_BUILD_TOOLS		coyotos_Constructor_APP_BUILD_TOOLS
#define CR_BUILD_PROTOSPACE 	coyotos_Constructor_APP_BUILD_PROTOSPACE
#define CR_DISCRIM		coyotos_Constructor_APP_DISCRIM
#define CR_SB_VERIFIER		coyotos_Constructor_APP_SB_VERIFIER
#define CR_TMP			coyotos_Constructor_APP_TMP

typedef union {
  _IDL_IFUNION_coyotos_Verifier
      coyotos_Verifier;
  _IDL_IFUNION_coyotos_Constructor
      coyotos_Constructor;
  _IDL_IFUNION_coyotos_Builder
      coyotos_Builder;
  InvParameterBlock_t pb;
  InvExceptionParameterBlock_t except;
  uintptr_t icw;
} _IDL_GRAND_SERVER_UNION;

bool isSealed = false;
bool isConfined = true;

typedef struct IDL_SERVER_Environment {
  bool isBuilder;
  bool isConstructor;
  bool haveReturn;	/**< @brief can we invoke CR_RETURN? */
} ISE;

ISE constructor_ISE = {
};

static bool 
cap_is_confined(caploc_t cap)
{
  bool result = false;
  bool isMe = false;
  coyotos_Cap_payload_t pp = 0;
  unsigned long long epID = 0;

  // is it inherently discreet?
  if (!coyotos_Discrim_isDiscreet(CR_DISCRIM, cap, &result))
    return false;

  if (result)
    return true;

  // is it the SpaceBank verifier?
  if (!coyotos_Discrim_compare(CR_DISCRIM, cap, CR_SB_VERIFIER, &result))
    return false;

  if (result)
    return true;

  // Is it a Constructor?
  if (!coyotos_Process_identifyEntry(CR_SELF, cap, &pp, &epID,
				     &isMe, &result))
    return false;

  // if none of the above, we're not confined.
  if (!result)
    return false;

  // verifiers cannot leak data
  if (pp == coyotos_Constructor_PP_Verifier)
    return true;

  // but Builders can
  if (pp != coyotos_Constructor_PP_Constructor)
    return false;

  // don't want to invoke myself, so short-circuit the test in that case
  if (isMe)
    return (isSealed && isConfined);

  // Ask the Constructor its opinion
  if (!coyotos_Constructor_isYieldConfined(cap, &result))
    return false;

  return (result);
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Cap_destroy(ISE *ise)
{
  /* Only builder caps can destroy the constructor. */
  if (!ise->isBuilder)
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
  if (_env->isBuilder)
    *out = IKT_coyotos_Builder;
  else if (_env->isConstructor)
    *out = IKT_coyotos_Constructor;
  else
    *out = IKT_coyotos_Verifier;
  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Constructor_isYieldConfined(bool *out, ISE *_env)
{
  *out = isConfined;
  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Constructor_create(caploc_t bank, caploc_t sched, 
				  caploc_t runtime, caploc_t retVal, 
				  ISE *_env)
{
  bool success = false;

  if (!coyotos_SpaceBank_verifyBank(CR_SB_VERIFIER, bank, &success) ||
      !success) {
    return (RC_coyotos_Cap_RequestError);
  }

  if (!coyotos_SpaceBank_allocProcess(bank, 
				      CR_YIELD_BRAND,
				      CR_NEW_PROC) ||
      !coyotos_SpaceBank_alloc(bank, 
			       coyotos_Range_obType_otEndpoint,
			       coyotos_Range_obType_otEndpoint,
			       coyotos_Range_obType_otInvalid,
			       CR_NEW_ENDPT,
			       CR_NEW_RENDPT,
			       CR_NULL)) {
    errcode_t err = IDL_exceptCode;
      
    (void) coyotos_Cap_destroy(bank);
    return (err);
  }

  // when state grows to more than FC + faultInfo, this will need to change.
  // 8 is the ProtoSpace entry point.
  if (!coyotos_Process_setState(CR_NEW_PROC, coyotos_Process_FC_Startup, 8)) {
    errcode_t err = IDL_exceptCode;
      
    (void) coyotos_Cap_destroy(bank);
    return (err);
  }

  if (!coyotos_Endpoint_setRecipient(CR_NEW_ENDPT, CR_NEW_PROC) ||
      !coyotos_Endpoint_setRecipient(CR_NEW_RENDPT, CR_NEW_PROC) ||
      !coyotos_Endpoint_setPayloadMatch(CR_NEW_RENDPT) ||
      !coyotos_Process_setSlot(CR_NEW_PROC, 
			       coyotos_Process_cslot_addrSpace,
			       CR_YIELD_PROTOSPACE) ||
      !coyotos_Process_setSlot(CR_NEW_PROC, 
			       coyotos_Process_cslot_schedule,
			       sched) ||
      !coyotos_Process_setCapReg(CR_NEW_PROC, 
				 CR_REPLYEPT.fld.loc, CR_NEW_RENDPT) ||
      !coyotos_Process_setCapReg(CR_NEW_PROC, 
				 CR_SELF.fld.loc, CR_NEW_PROC) ||
      !coyotos_Process_setCapReg(CR_NEW_PROC, 
				 CR_SPACEBANK.fld.loc, bank) ||
      !coyotos_Process_setCapReg(CR_NEW_PROC, 
			      CR_TOOLS.fld.loc, CR_YIELD_TOOLS) ||
      !coyotos_Process_setCapReg(CR_NEW_PROC, 
			      CR_INITEPT.fld.loc, CR_NEW_ENDPT) ||
      !coyotos_Process_setCapReg(CR_NEW_PROC, 
			      CR_RUNTIME.fld.loc, runtime) ||
      /* set up the protospace arguments */
      !coyotos_Process_setCapReg(CR_NEW_PROC,
	coyotos_Constructor_PROTOAPP_ADDRSPACE.fld.loc, 
	CR_YIELD_ADDRSPACE) ||
      !coyotos_Process_setCapReg(CR_NEW_PROC,
	coyotos_Constructor_PROTOAPP_HANDLER.fld.loc, 
	CR_YIELD_HANDLER) ||
      !coyotos_Process_setCapReg(CR_NEW_PROC,
	coyotos_Constructor_PROTOAPP_SCHEDULE.fld.loc, 
        sched) ||
      /* set it up to return to our caller */
      !coyotos_Process_setCapReg(CR_NEW_PROC, 
			      CR_RETURN.fld.loc, 
			      _env->haveReturn ? CR_RETURN : CR_NULL) ||
      /* set it running */
      !coyotos_Process_resume(CR_NEW_PROC, 0)) {
    errcode_t err = IDL_exceptCode;
      
    (void) coyotos_Cap_destroy(bank);
    return (err);
  }

  /* Do not return to our caller; we've handed CR_RETURN to our new child */
  _env->haveReturn = 0;

  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Constructor_getVerifier(caploc_t retVal, ISE *_env)
{
  if (!coyotos_Endpoint_makeEntryCap(CR_INITEPT, 
				     coyotos_Constructor_PP_Verifier, 
				     retVal))
    return RC_coyotos_Cap_RequestError;

  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Verifier_verifyYield(caploc_t cap, bool *result, ISE *_env)
{
  bool identifyResult = false;
  coyotos_Cap_payload_t pp = 0;
  unsigned long long epID = 0;
  bool isMe = false;

  if (!coyotos_Process_identifyEntry(CR_VERIFIER, cap, &pp, &epID,
				     &isMe, &identifyResult)) {
    return IDL_exceptCode;
  }
  *result = identifyResult;
  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Builder_setHandler(caploc_t handler, ISE *_env)
{
  if (isSealed)
    return (RC_coyotos_Builder_Sealed);

  if (!cap_is_confined(handler))
    isConfined = false;

  cap_copy(CR_YIELD_HANDLER, handler);

  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Builder_setSpace(caploc_t space, ISE *_env)
{
  if (isSealed)
    return (RC_coyotos_Builder_Sealed);

  if (!cap_is_confined(space))
    isConfined = false;

  cap_copy(CR_YIELD_ADDRSPACE, space);

  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Builder_setPC(coyotos_Cap_coyaddr_t pc, ISE *_env)
{
  enum {
    PROTOSPACE_SLOT = coyotos_GPT_nSlots - 2,
    PROTOSPACE_ADDR = COYOTOS_PAGE_SIZE * PROTOSPACE_SLOT
  };

  if (isSealed)
    return (RC_coyotos_Builder_Sealed);

  /* temporarily map the ProtoSpace page so that we can overwrite it */
  if (!coyotos_Process_getSlot(CR_SELF, coyotos_Process_cslot_addrSpace,
			       CR_TMP) ||
      !coyotos_AddressSpace_setSlot(CR_TMP, PROTOSPACE_SLOT,
				 CR_BUILD_PROTOSPACE))
    return (IDL_exceptCode);

  *(uint64_t *)PROTOSPACE_ADDR = pc;

  (void) coyotos_AddressSpace_setSlot(CR_TMP, PROTOSPACE_SLOT, CR_NULL);

  return (RC_coyotos_Cap_UnknownRequest);
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Builder_setTool(uint32_t slot, caploc_t tool, ISE *_env)
{
  if (isSealed)
    return (RC_coyotos_Builder_Sealed);

  if (!cap_is_confined(tool))
    isConfined = false;

  if (!coyotos_AddressSpace_setSlot(CR_BUILD_TOOLS, slot, tool))
    return IDL_exceptCode;

  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_Builder_seal(caploc_t _retVal, ISE *_env)
{
  if (!coyotos_Endpoint_makeEntryCap(CR_INITEPT, 
				     coyotos_Constructor_PP_Constructor, 
				     _retVal))
    return RC_coyotos_Cap_RequestError;

  /* null out the writable versions of Tools and ProtoSpace */
  cap_copy(CR_BUILD_TOOLS, CR_NULL);
  cap_copy(CR_BUILD_PROTOSPACE, CR_NULL);

  isSealed = 1;

  return RC_coyotos_Cap_OK;
}

/* You should supply a function that selects an interface
 * type based on the incoming endpoint ID and protected
 * payload */
static inline uint64_t 
choose_if(uint64_t epID, uint32_t pp)
{
  switch (pp) {
  case coyotos_Constructor_PP_Builder:
    return IKT_coyotos_Builder;

  case coyotos_Constructor_PP_Constructor:
    return IKT_coyotos_Constructor;

  case coyotos_Constructor_PP_Verifier:
    return IKT_coyotos_Verifier;

  default:
    return IKT_coyotos_Cap;
  }
}

bool
initialize(void)
{
  /* pull out the tools we need */
  if (!coyotos_AddressSpace_getSlot(CR_TOOLS, TOOL_DISCRIM, CR_DISCRIM))
    goto fail;

  if (!coyotos_AddressSpace_getSlot(CR_TOOLS, 
				    TOOL_SPACEBANK_VERIFY,
				    CR_SB_VERIFIER))
    goto fail;

  coyotos_Discrim_capClass class;

  /* Check to see if we're already sealed; if the BRAND is set up, we are */
  if (!coyotos_Discrim_classify(CR_DISCRIM, CR_YIELD_BRAND, &class))
    goto fail;

  if (class != coyotos_Discrim_capClass_clNull) {
    isSealed = 1;

    /* check to see if we are confined */
    if (!cap_is_confined(CR_YIELD_PROTOSPACE) ||
	!cap_is_confined(CR_YIELD_ADDRSPACE) ||
	!cap_is_confined(CR_YIELD_HANDLER))
      isConfined = false;

    size_t idx = 0;
    while (coyotos_AddressSpace_getSlot(CR_YIELD_TOOLS, idx, CR_TMP)) {
      if (!cap_is_confined(CR_TMP))
	isConfined = false;
      idx++;
    }

    return true;  /* all set up; no need to send an entry cap */
  }

  /* We were Constructed from MetaConstructor;  set up as a Builder */

  /* allocate a capPage and a Page, for ProtoSpace */
  if (!coyotos_SpaceBank_alloc(CR_SPACEBANK,
			       coyotos_Range_obType_otPage,
			       coyotos_Range_obType_otCapPage,
			       coyotos_Range_obType_otInvalid,
			       CR_BUILD_PROTOSPACE,
			       CR_BUILD_TOOLS,
			       CR_NULL) ||
      /* Pull out ProtoSpace, make a writable copy in CR_BUILD_PROTOSPACE,
       * and a read-only cap to the copy in CR_YIELD_PROTOSPACE
       */
      !coyotos_AddressSpace_getSlot(CR_TOOLS, 
				    TOOL_PROTOSPACE, 
				    CR_YIELD_PROTOSPACE) ||
      !coyotos_AddressSpace_copyFrom(CR_BUILD_PROTOSPACE, 
				     CR_YIELD_PROTOSPACE,
				     CR_NULL) ||
      !coyotos_Memory_reduce(CR_BUILD_PROTOSPACE, 
			     coyotos_Memory_restrictions_readOnly,
			     CR_YIELD_PROTOSPACE) ||
      /* Make a writable copy of TOOLs in CR_BUILD_TOOLS, and a read-only
       * version in CR_YIELD_TOOLS
       */
      !coyotos_AddressSpace_copyFrom(CR_BUILD_TOOLS,
				     CR_TOOLS, 
				     CR_NULL) ||
      !coyotos_Memory_reduce(CR_BUILD_TOOLS, 
			     coyotos_Memory_restrictions_readOnly,
			     CR_YIELD_TOOLS) ||
      /* Set up our brand, and allocate a Process cap as a Verifier for it */
      !coyotos_Endpoint_makeEntryCap(CR_INITEPT, 
				     coyotos_Constructor_PP_BRAND, 
				     CR_YIELD_BRAND) ||
      !coyotos_SpaceBank_allocProcess(CR_SPACEBANK,
				      CR_YIELD_BRAND,
				      CR_VERIFIER) ||
      /* Set up our Builder entry point */
      !coyotos_Endpoint_makeEntryCap(CR_INITEPT, 
				     coyotos_Constructor_PP_Builder, 
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

  /* no send phase for initial invocation */
  gsu.icw = 0;

  _env->haveReturn = 0;

  /* set up unchanging recieve state */
  gsu.pb.rcvCap[0] = CR_RETURN;
  gsu.pb.rcvCap[1] = CR_ARG(0);
  gsu.pb.rcvCap[2] = CR_ARG(1);
  gsu.pb.rcvCap[3] = CR_ARG(2);
  gsu.pb.rcvBound = (sizeof(gsu) - sizeof(gsu.pb));
  gsu.pb.rcvPtr = ((char *)(&gsu)) + sizeof(gsu.pb);  

  gsu.pb.sndLen = 0;
  gsu.pb.sndPtr = 0;

  for(;;) {
    gsu.icw &= (IPW0_LDW_MASK|IPW0_LSC_MASK
        |IPW0_SG|IPW0_SP|IPW0_RC|IPW0_SC|IPW0_EX);
    gsu.icw |= IPW0_MAKE_NR(sc_InvokeCap)|IPW0_RP|IPW0_AC
        |IPW0_MAKE_LRC(3)|IPW0_NB|IPW0_CO;

    gsu.pb.u.invCap = (_env->haveReturn)? CR_RETURN : CR_NULL;

    invoke_capability(&gsu.pb);

    /* reset send parameters, except for sndLen */
    gsu.pb.sndPtr = 0;
    gsu.pb.sndCap[0] = CR_REPLY0;
    gsu.pb.sndCap[1] = CR_REPLY1;
    gsu.pb.sndCap[2] = CR_REPLY2;
    gsu.pb.sndCap[3] = CR_REPLY3;

    /* Check if they sent us a return cap */
    _env->haveReturn = !!(gsu.icw & IPW0_SC);

    /* set up type based on pp */
    _env->isBuilder = (gsu.pb.u.pp == coyotos_Constructor_PP_Builder);
    _env->isConstructor = (gsu.pb.u.pp == coyotos_Constructor_PP_Constructor);

    /* and finally, dispatch the request. */
    switch(choose_if(gsu.pb.epID, gsu.pb.u.pp)) {
    case IKT_coyotos_Verifier:
      _IDL_IFDISPATCH_coyotos_Verifier(&gsu.coyotos_Verifier, _env);
      break;
    case IKT_coyotos_Constructor:
      _IDL_IFDISPATCH_coyotos_Constructor(&gsu.coyotos_Constructor, _env);
      break;
    case IKT_coyotos_Builder:
      _IDL_IFDISPATCH_coyotos_Builder(&gsu.coyotos_Builder, _env);
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
  if (!initialize())
    return (0);

  ProcessRequests(&constructor_ISE);

  return 0;
}
