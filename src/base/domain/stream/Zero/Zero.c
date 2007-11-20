/* Example template for processing the following interfaces:
    coyotos.IoStream
 */

#include <string.h>

#include <coyotos/capidl.h>
#include <coyotos/syscall.h>
#include <coyotos/runtime.h>
#include <coyotos/kprintf.h>
#include <coyotos/reply_create.h>

#include <idl/coyotos/SpaceBank.h>
#include <idl/coyotos/Endpoint.h>
#include <idl/coyotos/AddressSpace.h>

#define IDL_SERVER_CLEANUP_PREDECL static inline
#define IDL_SERVER_HANDLER_PREDECL static inline

#include <idl/coyotos/IoStream.server.h>
#include <coyotos.stream.Zero.h>

bool isClosed = false;

/* Utility quasi-syntax */
#define unless(x) if (!(x))

#define CR_LOG   		coyotos_stream_Zero_APP_Log
#define CR_MYENTRY   		coyotos_stream_Zero_APP_MyEntry

#define TOOL_LOG 		coyotos_stream_Zero_TOOL_Log

typedef union {
  _IDL_IFUNION_coyotos_IoStream
      coyotos_IoStream;
  InvParameterBlock_t pb;
  InvExceptionParameterBlock_t except;
  uintptr_t icw;
} _IDL_GRAND_SERVER_UNION;

extern uint64_t choose_if(uint64_t epID, uint32_t pp)
{
  /* All of the request channels have the same interface. */
  return IKT_coyotos_IoStream;
}

static char zeroBuffer[coyotos_IoStream_bufLimit];

IDL_SERVER_HANDLER_PREDECL uint64_t 
HANDLE_coyotos_IoStream_doRead(
  uint32_t length,
  coyotos_IoStream_chString *s,
  struct IDL_SERVER_Environment *_env)
{
  if (isClosed)
    return RC_coyotos_IoStream_Closed;

  if (length > coyotos_IoStream_bufLimit)
    return RC_coyotos_Cap_RequestError;

  /* All reads from NULL are accepted and return zero length. */
  s->len = length;
  s->data = zeroBuffer;

  return RC_coyotos_Cap_OK;
}

IDL_SERVER_CLEANUP_PREDECL void
CLEANUP_coyotos_IoStream_doRead(
  coyotos_IoStream_chString s,
  struct IDL_SERVER_Environment *_env)
{
  /* No actual buffer, so nothing to do. */
}

IDL_SERVER_HANDLER_PREDECL uint64_t
HANDLE_coyotos_IoStream_doWrite(
  coyotos_IoStream_chString s,
  uint32_t *_retVal,
  struct IDL_SERVER_Environment *_env)
{
  if (isClosed)
    return RC_coyotos_IoStream_Closed;

  *_retVal = s.len;

  /* All writes to NULL are accepted and ignored. */
  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t 
HANDLE_coyotos_IoStream_getReadChannel(
  caploc_t _retVal,
  struct IDL_SERVER_Environment *_env)
{
  /* Since Zero never blocks, there is no need to actually maintain
     multiple channels. We can adopt the expedience of returning the
     common channel here. */

  cap_copy(_retVal, CR_MYENTRY);
  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t 
HANDLE_coyotos_IoStream_getWriteChannel(
  caploc_t _retVal,
  struct IDL_SERVER_Environment *_env)
{
  /* Since Zero never blocks, there is no need to actually maintain
     multiple channels. We can adopt the expedience of returning the
     common channel here. */

  cap_copy(_retVal, CR_MYENTRY);
  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t 
HANDLE_coyotos_IoStream_close(
  struct IDL_SERVER_Environment *_env)
{
  if (isClosed)
    return RC_coyotos_IoStream_Closed;

  isClosed = true;

  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t 
HANDLE_coyotos_Cap_getType(
  uint64_t *_retVal,
  struct IDL_SERVER_Environment *_env)
{
  *_retVal = IKT_coyotos_IoStream;

  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t 
HANDLE_coyotos_Cap_destroy(
  struct IDL_SERVER_Environment *_env)
{
  coyotos_SpaceBank_destroyBankAndReturn(CR_SPACEBANK,
					 CR_RETURN,
					 RC_coyotos_Cap_OK);
  /* Not Reached */
  return (IDL_exceptCode);
}

typedef struct IDL_SERVER_Environment {
  uint32_t pp;
  uint64_t epID;
} IDL_SERVER_Environment;

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
    
    _env->pp = gsu.pb.u.pp;
    _env->epID = gsu.pb.epID;
    
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
    
    switch(choose_if(gsu.pb.epID, gsu.pb.u.pp)) {
    case IKT_coyotos_IoStream:
      _IDL_IFDISPATCH_coyotos_IoStream(&gsu.coyotos_IoStream, _env);
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

static inline bool
exit_gracelessly(errcode_t  errCode)
{
  kprintf(CR_LOG, "Exiting -- startup error\n");
  return
    coyotos_SpaceBank_destroyBankAndReturn(CR_SPACEBANK, 
					   CR_RETURN,
					   errCode);
}

bool 
initialize()
{
  memset(zeroBuffer, 0, sizeof(zeroBuffer));

  unless (
	  coyotos_AddressSpace_getSlot(CR_TOOLS, TOOL_LOG, CR_LOG) &&

	  /* Set up our entry capability */
	  coyotos_Endpoint_makeEntryCap(CR_INITEPT,
					coyotos_stream_Zero_PP_common,
					CR_MYENTRY)
	  )
    exit_gracelessly(IDL_exceptCode);

  /* Send our entry cap to our caller */
  REPLY_create(CR_RETURN, CR_MYENTRY);

  return true;
}

int
main(int argc, char *argv[])
{
  struct IDL_SERVER_Environment ise;

  if (!initialize())
    return (0);

  ProcessRequests(&ise);

  return 0;
}
