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
 * @brief SpaceBank implementation
 */

#include "SpaceBank.h"
#include <idl/coyotos/SpaceBank.server.h>

/** @brief Initially, we just want a recieve phase that accepts 4 caps */
#define INITIAL_IPW0 IPW0_CO | IPW0_RP | IPW0_AC | IPW0_MAKE_LRC(3)

#define REPLY_IPW0_NOLDW \
  INITIAL_IPW0 | IPW0_SP | IPW0_NB 

/** @brief Normal reply, @p ldw reply words, no capabilities */
#define REPLY_IPW0(ldw) \
  REPLY_IPW0_NOLDW | IPW0_MAKE_LDW(ldw)

/** @brief Normal reply, @p ldw reply words, @p lastcap + 1 capabilities */
#define REPLY_IPW0_CAP(ldw, lastcap) \
  REPLY_IPW0(ldw) | IPW0_SC | IPW0_MAKE_LSC(lastcap)

/** @brief Process a single request, rewriting @p pb to do the reply.
 * @p limits is the recieve buffer. 
 */
static void
process_request(InvParameterBlock_t *pb)
{
  coyotos_SpaceBank_limits *limits = pb->rcvPtr;

  // pull everything we need to know about out before dorking with the
  // structure.
  
  uintptr_t opw0 = pb->pw[0];
  
  /* number of argument caps, not including the reply cap */
  unsigned caps = (opw0 & IPW0_SC) ? IPW0_LSC(opw0) : 0;
  /* number of argument data words, not including OPW0 */
  unsigned data = IPW0_LDW(opw0) - 1;
  unsigned edata = pb->sndLen;

  Bank *bank = bank_fromEPID(pb->epID);
  uint32_t restr = pb->u.pp;

  assert(bank != 0);

  // set up the basic reply
  pb->pw[0] = REPLY_IPW0(0); // default to no DW, no caps

  // Only reply to RETURN if one was passed.
  pb->u.invCap = (opw0 & IPW0_SC) ? CR_RETURN : CR_NULL;
  pb->sndLen = 0;
  pb->sndCap[0] = CR_NULL;
  pb->sndCap[1] = CR_NULL;
  pb->sndCap[2] = CR_NULL;
  pb->sndCap[3] = CR_NULL;
  
  // don't accept exceptional invocations, or ones without order codes
  if ((opw0 & IPW0_EX) || IPW0_LDW(opw0) == 0)
    goto unknown_request;

  /** @bug set up generic return: no caps, zeroed return values */

  switch (pb->pw[1]) {
  case OC_coyotos_SpaceBank_alloc: {
    if (restr & coyotos_SpaceBank_restrictions_noAlloc)
      goto no_access;
      
    if (data != 3 || edata > 0 ||  caps > 0)
      goto bad_request;

    Object *obj1 = 0;
    Object *obj2 = 0;
    Object *obj3 = 0;
    coyotos_Range_obType ty1 = pb->pw[2];
    coyotos_Range_obType ty2 = pb->pw[3];
    coyotos_Range_obType ty3 = pb->pw[4];

    pb->pw[0] = REPLY_IPW0_CAP(0, 2); // no data, three caps
    
    if (!(check_obType(ty1) || check_obType(ty2) || check_obType(ty3)))
      goto bad_request;
    
    if (ty1 != coyotos_Range_obType_otInvalid) {
      if ((obj1 = bank_alloc(bank, ty1, CR_REPLY0)) == 0)
	goto fail_alloc;

      pb->sndCap[0] = CR_REPLY0;
    }

    if (ty2 != coyotos_Range_obType_otInvalid) {
      if ((obj2 = bank_alloc(bank, ty2, CR_REPLY1)) == 0)
	goto fail_alloc;
      
      pb->sndCap[1] = CR_REPLY1;
    }

    if (ty3 != coyotos_Range_obType_otInvalid) {
      if ((obj3 = bank_alloc(bank, ty3, CR_REPLY2)) == 0)
	goto fail_alloc;
      
      pb->sndCap[2] = CR_REPLY2;
    }
    return;
    
    fail_alloc:
    if (obj3 != 0)
      object_unalloc(obj3);
    if (obj2 != 0)
      object_unalloc(obj2);
    if (obj1 != 0)
      object_unalloc(obj1);
    
    goto limit_reached;
  }
  case OC_coyotos_SpaceBank_free: {
    if (restr & coyotos_SpaceBank_restrictions_noFree)
      goto no_access;

    // Must have 0 data, 3 argument caps
    if (data != 0 || edata != 0 || caps != 3)
      goto bad_request;
    
    Object *obj1 = object_identify(CR_ARG0);
    Object *obj2 = object_identify(CR_ARG1);
    Object *obj3 = object_identify(CR_ARG2);
    
    if ((obj1 != 0 && obj1->bank != bank) ||
	(obj2 != 0 && obj2->bank != bank) ||
	(obj3 != 0 && obj3->bank != bank))
      goto bad_request;
    
    if (obj1 != 0) object_rescindAndFree(obj1);
    if (obj2 != 0) object_rescindAndFree(obj2);
    if (obj3 != 0) object_rescindAndFree(obj3);
    
    /* return success */
    return;
  }
  case OC_coyotos_SpaceBank_allocProcess:
    if (restr & coyotos_SpaceBank_restrictions_noAlloc)
      goto no_access;
    
    if (data != 0 || edata != 0 || caps != 1)
      goto bad_request;
    
    pb->pw[0] = REPLY_IPW0_CAP(0, 0); // no data, one
    
    Object *obj = bank_alloc_proc(bank, CR_ARG0, CR_REPLY0);
    
    if (obj == 0)
      goto limit_reached;
    
    pb->sndCap[0] = CR_REPLY0;
    return;

  case OC_coyotos_SpaceBank_setLimits:
    if (restr & coyotos_SpaceBank_restrictions_noChangeLimits)
      goto no_access;
    
    if (data != 0 || edata != sizeof (*limits) || caps != 0)
      goto bad_request;
    
    if (!bank_setLimits(bank, limits))
      goto limit_reached;
    
    // success!
    return;

  case OC_coyotos_SpaceBank_getLimits:
    if (restr & coyotos_SpaceBank_restrictions_noQueryLimits)
      goto no_access;
    
    if (data != 0 || edata != 0 || caps != 0)
      goto bad_request;
    
    bank_getLimits(bank, limits);
    pb->sndPtr = limits;
    pb->sndLen = sizeof (*limits);
    return;

  case OC_coyotos_SpaceBank_getUsage:
    if (restr & coyotos_SpaceBank_restrictions_noQueryLimits)
      goto no_access;
    
    if (data != 0 || edata != 0 || caps != 0)
      goto bad_request;
    
    bank_getUsage(bank, limits);
    pb->sndPtr = limits;
    pb->sndLen = sizeof (*limits);
    return;

  case OC_coyotos_SpaceBank_getEffectiveLimits:
    if (restr & coyotos_SpaceBank_restrictions_noQueryLimits)
      goto no_access;
    
    if (data != 0 || edata != 0 || caps != 0)
      goto bad_request;
    
    bank_getEffLimits(bank, limits);
    pb->sndPtr = limits;
    pb->sndLen = sizeof (*limits);
    return;
    
  case OC_coyotos_SpaceBank_createChild:
    if (restr & coyotos_SpaceBank_restrictions_noAlloc)
      goto no_access;
    
    if (data != 0 || edata != 0 || caps != 0)
      goto bad_request;
    
    if (!bank_create(bank, CR_REPLY0))
      goto limit_reached;
    
    pb->pw[0] = REPLY_IPW0_CAP(0, 0); // one cap
    pb->sndCap[0] = CR_REPLY0;
    return;

  case OC_coyotos_SpaceBank_verifyBank: {
    bool success = false;
    uint64_t epID;
    coyotos_Cap_payload_t payload;
    bool isMe = false;

    if (data != 0 || edata != 0 || caps != 1)
      goto bad_request;
        
    MUST_SUCCEED(coyotos_Process_identifyEntry(CR_SELF, CR_ARG0,
					       &payload, 
					       &epID, &isMe,
					       &success));
    
    bool result = (success && isMe);
    pb->pw[1] = result;
    pb->pw[0] = REPLY_IPW0(1);
    return;
  }
    
  case OC_coyotos_SpaceBank_reduce: {

    if (data != 1 || edata != 0 || caps != 0)
      goto bad_request;

    coyotos_SpaceBank_restrictions newRestr = pb->pw[2];

    // make sure we don't include non-sensical restrictions
    if (newRestr > coyotos_SpaceBank_restrictions_noRemove)
      goto bad_request;
    
    bank_getEntry(bank, restr | newRestr, CR_REPLY0);
    pb->sndCap[0] = CR_REPLY0;
    pb->pw[0] = REPLY_IPW0_CAP(0, 0); // no payload, 1 cap
    return;
  }

  case OC_coyotos_SpaceBank_destroyBankAndReturn: {
    if (restr & coyotos_SpaceBank_restrictions_noDestroy)
      goto no_access;

    // special case the Prime Bank, since no-one can destroy it.
    if (bank->parent == 0)
      goto no_access;

    if (data != (sizeof (coyotos_Cap_exception_t) / sizeof (uintptr_t)))
      goto bad_request;

    if (edata != 0 || caps != 1)
      goto bad_request;

    coyotos_Cap_exception_t ex = *(coyotos_Cap_exception_t *)&pb->pw[2];

    bank_destroy(bank, 1);  // destroy objects as well

    /* now that we've succeeded, re-direct the reply to ARG0 */
    pb->u.invCap = CR_ARG0;

    /* use the passed exception */
    if (ex != RC_coyotos_Cap_OK) {
	invoke_setErrorReply(pb, ex);
	pb->pw[0] |= REPLY_IPW0_NOLDW;
    }
    
    return;
  }

  case OC_coyotos_SpaceBank_remove:
    if (restr & coyotos_SpaceBank_restrictions_noDestroy)
      goto no_access;
    if (restr & coyotos_SpaceBank_restrictions_noRemove)
      goto no_access;

    // special case the Prime Bank, since no-one can destroy it.
    if (bank->parent == 0)
      goto no_access;

    if (data != 0 || edata != 0 || caps != 0)
      goto bad_request;
    
    bank_destroy(bank, 0);  // move the objects into the bank's parent
    return;

  case OC_coyotos_Cap_destroy:
    if (restr & coyotos_SpaceBank_restrictions_noDestroy)
      goto no_access;
    
    // special case the Prime Bank, since no-one can destroy it.
    if (bank->parent == 0)
      goto no_access;
    
    if (data != 0 || edata != 0 || caps != 0)
      goto bad_request;
    
    bank_destroy(bank, 1);  // destroy objects as well

    return;
    
  case OC_coyotos_Cap_getType:

    if (data != 0 || edata != 0 || caps != 0)
      goto bad_request;
    
    /* Shap hack -- temporary */
    invoke_setErrorReply(pb, IKT_coyotos_SpaceBank);
    pb->pw[0] &= ~IPW0_EX;
    pb->pw[0] |= REPLY_IPW0_NOLDW;
    return;

  default:
    goto unknown_request;
  }

  /* Should never get here */
  assert(0 && "Should never get here");

 no_access:
  invoke_setErrorReply(pb, RC_coyotos_Cap_NoAccess);
  pb->pw[0] |= REPLY_IPW0_NOLDW;
  return;

 unknown_request:
  invoke_setErrorReply(pb, RC_coyotos_Cap_UnknownRequest);
  pb->pw[0] |= REPLY_IPW0_NOLDW;
  return;
  
 bad_request:
  invoke_setErrorReply(pb, RC_coyotos_Cap_RequestError);
  pb->pw[0] |= REPLY_IPW0_NOLDW;
  return;

 limit_reached:
  invoke_setErrorReply(pb, RC_coyotos_SpaceBank_LimitReached);
  pb->pw[0] |= REPLY_IPW0_NOLDW;
  return;
}

_IDL_IFUNION_coyotos_SpaceBank ipb = {
  ._pb = {
    .pw[0] = INITIAL_IPW0,
    .u.invCap = CR_NULL,
    .sndCap[0] = CR_NULL,
    .sndCap[1] = CR_NULL,
    .sndCap[2] = CR_NULL,
    .sndCap[3] = CR_NULL,
    .rcvCap[0] = CR_RETURN,
    .rcvCap[1] = CR_ARG0,
    .rcvCap[2] = CR_ARG1,
    .rcvCap[3] = CR_ARG2,

    .sndPtr = 0,
    .sndLen = 0,

    /** @bug Is there a better way to do this? */
    .rcvPtr = (char *)&ipb + sizeof (ipb._pb),
    .rcvBound = sizeof(ipb) - sizeof (ipb._pb),
  }
};

int
main(int argc, char *argv[])
{
  InvParameterBlock_t *pb = &ipb._pb;

  initialize();

  for (;;) {
    (void) invoke_capability(pb);

    process_request(pb);
  }
}
