#include <coyotos/syscall.h>
#include <idl/coyotos/Cap.h>

/* This currently uses an open wait. */
const InvParameterBlock_t ipc_parameter_block = {
  .pw[0] = (IPW0_RP|IPW0_CO|IPW0_RC|IPW0_SP+IPW0_SC+IPW0_MAKE_LSC(0)+IPW0_MAKE_LDW(1)+sc_InvokeCap),
  .pw[1] = OC_coyotos_Cap_getType,
  .u.invCap = CAP_REG(0),
  .sndCap[0] = CAP_REG(1),
  .epID = 0,
};
