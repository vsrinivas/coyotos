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
 * @brief Handler test case.
 *
 * This is a simple test case for checking whether the kernel Handler
 * invocation paths are working.
 *
 * It responds to both the MemoryHandler and ProcessHandler invocations,
 * and just prints a message to the log and goes back to waiting for requests.
 *
 * No attempt is made to do *anything* to the failing processes.
 */

#include <coyotos/capidl.h>
#include <coyotos/syscall.h>
#include <coyotos/runtime.h>

#include <string.h>

#include "testHandler.h"

#include <idl/coyotos/KernLog.h>

/* all of our handler procedures are static */
#define IDL_SERVER_HANDLER_PREDECL static

#include <idl/coyotos/MemoryHandler.server.h>
#include <idl/coyotos/ProcessHandler.server.h>

typedef union {
  _IDL_IFUNION_coyotos_MemoryHandler
      coyotos_MemoryHandler;
  _IDL_IFUNION_coyotos_ProcessHandler
      coyotos_ProcessHandler;
  InvParameterBlock_t pb;
  InvExceptionParameterBlock_t except;
  uintptr_t icw;
} _IDL_GRAND_SERVER_UNION;

typedef struct IDL_SERVER_Environment {
  uint64_t epID;
  uint32_t pp;
} ISE;

/* You should supply a function that selects an interface
 * type based on the incoming endpoint ID and protected
 * payload */
static inline uint64_t 
choose_if(uint64_t epID, uint32_t pp)
{
  if (epID != 1)
    return IKT_coyotos_Cap;
  if (pp == testHandler_PP_MemoryHandler)
    return IKT_coyotos_MemoryHandler;
  if (pp == testHandler_PP_ProcessHandler)
    return IKT_coyotos_ProcessHandler;

  return IKT_coyotos_Cap;
}

static void
log_message(const char *message)
{
  size_t len = strlen(message);
  if (len > 255) 
    len = 255;
  
  coyotos_KernLog_logString str = { 
    .max = 256, .len = len, .data = (char *)message
  };
  
  (void) coyotos_KernLog_log(testHandler_APP_KERNLOG, str);
}

static uint64_t
HANDLE_coyotos_Cap_destroy(ISE *ise)
{
  return (RC_coyotos_Cap_NoAccess);
}

static uint64_t
HANDLE_coyotos_Cap_getType(uint64_t *out, ISE *ise)
{
  *out = choose_if(ise->epID, ise->pp);
  if (*out == 0)
    *out = IKT_coyotos_Cap;
  return (RC_coyotos_Cap_OK);
}

static void
HANDLE_coyotos_ProcessHandler_handle(caploc_t proc,
				     coyotos_Process_FC faultCode,
				     uint64_t faultInfo,
				     struct IDL_SERVER_Environment *_env)
{
  log_message("ProcessHandler\n");
}

static void
HANDLE_coyotos_MemoryHandler_handle(caploc_t proc,
				    coyotos_Process_FC faultCode,
				    uint64_t faultInfo,
				    struct IDL_SERVER_Environment *_env)
{
  log_message("MemoryHandler\n");  
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
    /* we never send a reply, since we want the processes which fault to
       stop
    */
    gsu.icw = 0;

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
    
    _env->pp = gsu.pb.u.pp;
    _env->epID = gsu.pb.epID;

    switch(choose_if(gsu.pb.epID, gsu.pb.u.pp)) {
    case IKT_coyotos_MemoryHandler:
      _IDL_IFDISPATCH_coyotos_MemoryHandler(&gsu.coyotos_MemoryHandler, _env);
      break;
    case IKT_coyotos_ProcessHandler:
      _IDL_IFDISPATCH_coyotos_ProcessHandler(&gsu.coyotos_ProcessHandler, _env);
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
  ISE env;
  ProcessRequests(&env);
  return 0;
}
