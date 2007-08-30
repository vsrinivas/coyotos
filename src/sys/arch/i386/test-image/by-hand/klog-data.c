#include <coyotos/syscall.h>
#include <idl/coyotos/Cap.h>
#include <idl/coyotos/KernLog.h>

#define MYSTR "This is a test of the Kernel Log Cap!\n"

/* This currently uses an open wait. */
_NEW_INV_coyotos_KernLog_log ipc_parameter_block = { .in = {
  ._icw = IPW0_RP|IPW0_CO|IPW0_RC|IPW0_SP+IPW0_SC+IPW0_MAKE_LSC(0)+IPW0_MAKE_LDW(4)+sc_InvokeCap,
  ._opCode = OC_coyotos_KernLog_log,
  ._invCap = CAP_REG(2),
  ._replyCap = CAP_REG(1),
  .msg = {.max = 256, .len = sizeof (MYSTR), .data = 0 },
  ._sndPtr = &ipc_parameter_block.in._indirect[0],
  ._sndLen = sizeof(MYSTR),
  ._epID = 0,
  ._indirect = MYSTR,
}
};
