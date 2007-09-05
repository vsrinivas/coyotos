#include <coyotos/syscall.h>
#include <coyotos/runtime.h>
#include <coyotos/machine/pagesize.h>
#include <idl/coyotos/AddressSpace.h>
#include <idl/coyotos/GPT.h>
#include <idl/coyotos/Process.h>
#include <idl/coyotos/Range.h>
#include <idl/coyotos/SpaceBank.h>
#include "coyotos.TargetInfo.h"

#define CR_NEWADDR	CR_APP(0)
#define CR_NEWPAGE	CR_APP(1)
#define CR_OLDADDR	CR_APP(2)

#define MY_IPW0(dw, sc) \
    (IPW0_CW|IPW0_SP|IPW0_RP|IPW0_RC|IPW0_SC| \
     IPW0_MAKE_NR(sc_InvokeCap) | IPW0_MAKE_LDW(dw) | IPW0_MAKE_LSC(sc))

#define MY_IPW0_RC(dw, sc, rc) (MY_IPW0(dw, sc) | IPW0_AC | IPW0_MAKE_LRC(rc))

const InvParameterBlock_t __small_runtime_hook_call_1 = {
  .pw[0] = MY_IPW0_RC(4, 0, 1),
  .pw[1] = OC_coyotos_SpaceBank_alloc,
  .pw[2] = coyotos_Range_obType_otGPT,
  .pw[3] = coyotos_Range_obType_otPage,
  .pw[4] = coyotos_Range_obType_otInvalid,
  .u.invCap = CR_SPACEBANK,
  .sndCap[0] = CR_REPLYEPT,
  .rcvCap[0] = CR_NEWADDR,
  .rcvCap[1] = CR_NEWPAGE,
  .epID = 0
};

const InvParameterBlock_t __small_runtime_hook_call_2 = {
  .pw[0] = MY_IPW0_RC(2, 0, 0),
  .pw[1] = OC_coyotos_Process_getSlot,
  .pw[2] = coyotos_Process_cslot_addrSpace,
  .u.invCap = CR_SELF,
  .sndCap[0] = CR_REPLYEPT,
  .rcvCap[0] = CR_OLDADDR,
  .epID = 0
};

const InvParameterBlock_t __small_runtime_hook_call_3 = {
  .pw[0] = MY_IPW0_RC(1, 1, 0),
  .pw[1] = OC_coyotos_AddressSpace_copyFrom,
  .u.invCap = CR_NEWADDR,
  .sndCap[0] = CR_REPLYEPT,
  .sndCap[1] = CR_OLDADDR,
  .rcvCap[0] = CR_NEWADDR,
  .epID = 0
};

const InvParameterBlock_t __small_runtime_hook_call_4 = {
  .pw[0] = MY_IPW0(2, 1),
  .pw[1] = OC_coyotos_AddressSpace_setSlot,
  .pw[2] = (coyotos_TargetInfo_small_stack_page_addr / COYOTOS_PAGE_SIZE),
  .u.invCap = CR_NEWADDR,
  .sndCap[0] = CR_REPLYEPT,
  .sndCap[1] = CR_NEWPAGE,
  .epID = 0
};

const InvParameterBlock_t __small_runtime_hook_call_5 = {
  .pw[0] = MY_IPW0(2, 1),
  .pw[1] = OC_coyotos_Process_setSlot,
  .pw[2] = coyotos_Process_cslot_addrSpace,
  .u.invCap = CR_SELF,
  .sndCap[0] = CR_REPLYEPT,
  .sndCap[1] = CR_NEWADDR,
  .epID = 0
};
