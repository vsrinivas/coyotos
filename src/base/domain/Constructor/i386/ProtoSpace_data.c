#include <coyotos/syscall.h>
#include <coyotos/runtime.h>
#include <coyotos/machine/pagesize.h>
#include <idl/coyotos/AddressSpace.h>
#include <idl/coyotos/Cap.h>
#include <idl/coyotos/Process.h>
#include <idl/coyotos/Constructor.h>
#include "coyotos.Constructor.h"

#define CR_ADDRSPACE	coyotos_Constructor_PROTOAPP_ADDRSPACE
#define CR_HANDLER	coyotos_Constructor_PROTOAPP_HANDLER
#define CR_SCHEDULE	coyotos_Constructor_PROTOAPP_SCHEDULE

#define MY_IPW0(dw, sc) \
    (IPW0_CW|IPW0_SP|IPW0_RP|IPW0_RC|IPW0_SC| \
     IPW0_MAKE_NR(sc_InvokeCap) | IPW0_MAKE_LDW(dw) | IPW0_MAKE_LSC(sc))

#define MY_IPW0_RC(dw, sc, rc) (MY_IPW0(dw, sc) | IPW0_AC | IPW0_MAKE_LRC(rc))

const InvParameterBlock_t handler_call_getType = {
  .pw[0] = MY_IPW0(1, 0),
  .pw[1] = OC_coyotos_Cap_getType,
  .u.invCap = CR_HANDLER,
  .sndCap[0] = CR_REPLYEPT,
  .epID = 0
};

const InvParameterBlock_t addrSpace_call_getType = {
  .pw[0] = MY_IPW0(1, 0),
  .pw[1] = OC_coyotos_Cap_getType,
  .u.invCap = CR_ADDRSPACE,
  .sndCap[0] = CR_REPLYEPT,
  .epID = 0
};

const InvParameterBlock_t handler_call_create = {
  .pw[0] = MY_IPW0_RC(1, 3, 0),
  .pw[1] = OC_coyotos_Constructor_create,
  .u.invCap = CR_HANDLER,
  .sndCap[0] = CR_REPLYEPT,
  .sndCap[1] = CR_SCHEDULE,
  .sndCap[2] = CR_NULL,
  .rcvCap[0] = CR_HANDLER,
  .epID = 0
};

const InvParameterBlock_t addrSpace_call_create = {
  .pw[0] = MY_IPW0_RC(1, 3, 0),
  .pw[1] = OC_coyotos_Constructor_create,
  .u.invCap = CR_ADDRSPACE,
  .sndCap[0] = CR_REPLYEPT,
  .sndCap[1] = CR_SCHEDULE,
  .sndCap[2] = CR_NULL,  /* do not pass along runtime cap */
  .rcvCap[0] = CR_ADDRSPACE,
  .epID = 0
};

const InvParameterBlock_t self_call_setSlot_handler = {
  .pw[0] = MY_IPW0(2, 1),
  .pw[1] = OC_coyotos_Process_setSlot,
  .pw[2] = coyotos_Process_cslot_handler,
  .u.invCap = CR_SELF,
  .sndCap[0] = CR_REPLYEPT,
  .sndCap[1] = CR_HANDLER,
  .epID = 0
};

const InvParameterBlock_t self_call_setSpaceAndPC = {
  .pw[0] = MY_IPW0(3, 1),
  .pw[1] = OC_coyotos_Process_setSpaceAndPC,
  .pw[2] = coyotos_Process_cslot_handler,
  .u.invCap = CR_SELF,
  .sndCap[0] = CR_REPLYEPT,
  .sndCap[1] = CR_ADDRSPACE,
  .epID = 0
};

const _INV_coyotos_SpaceBank_destroyBankAndReturn spaceBank_call_destroy ={
  .in = {
    ._icw = MY_IPW0(2, 1),
    ._opCode = OC_coyotos_SpaceBank_destroyBankAndReturn,
    .resultCode = RC_coyotos_SpaceBank_LimitReached,
    ._invCap = CR_SPACEBANK,
    ._replyCap = CR_REPLYEPT,
    .rcvr = CR_RETURN,
    ._epID = 0
  }
};
