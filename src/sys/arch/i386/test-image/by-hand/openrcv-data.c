#include <coyotos/syscall.h>
#include <idl/coyotos/Cap.h>

/* This currently uses an open wait. */
const InvParameterBlock_t ipc_parameter_block = {
  .pw[0] = (IPW0_RP+IPW0_AC+IPW0_MAKE_LRC(0)+IPW0_MAKE_LDW(0)+sc_InvokeCap),
  .epID = 0,
};
