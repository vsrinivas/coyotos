
#include <idl/coyotos/Builder.h>
#include <idl/coyotos/KernLog.h>

#include <coyotos/runtime.h>

#include "testConstructor.h"

#include <string.h>


IDL_Environment IDL_E = {
    .epID = 0,
    .replyCap = CR_REPLYEPT,
};

void
kernlog(const char *message)
{
  size_t len = strlen(message);
  
  if (len > 255) 
    len = 255;
  
  coyotos_KernLog_logString str = { 
    .max = 256, .len = len, .data = (char *)message
  };
  
  (void) coyotos_KernLog_log(testConstructor_REG_KERNLOG, str, &IDL_E);
}

int
main(int argc, char *argv[])
{
  uint64_t type = 0;

  if (!coyotos_Constructor_create(testConstructor_REG_METACON,
				  testConstructor_REG_BANK,
				  testConstructor_REG_SCHED,
				  testConstructor_REG_RUNTIME,
				  testConstructor_REG_TMP,
				  &IDL_E))
    kernlog("create failed\n");
  else if (!coyotos_Cap_getType(testConstructor_REG_TMP, &type, &IDL_E))
    kernlog("getType failed\n");

  else if (type != IKT_coyotos_Builder)
    kernlog("getType bad result\n");

  else
    kernlog("constructor IKT successful\n");
  
  *(uint64_t *)0 = 0;
  return 0;
}

