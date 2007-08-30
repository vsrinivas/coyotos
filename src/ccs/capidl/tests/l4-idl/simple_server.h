/*****************************************************************
 * DO NOT EDIT - MACHINE-GENERATED FILE!
 * 
 * Source file : simple.idl
 * Platform    : X0 IA32
 * Mapping     : CORBA C
 * 
 * Generated by IDL4 1.0.2 (roadrunner) on 02/08/2007 11:33
 * Report bugs to haeberlen@ira.uka.de
 *****************************************************************/

#if !defined(__simple_server_h__)
#define __simple_server_h__

#define IDL4_OMIT_FRAME_POINTER 0
#define IDL4_USE_FASTCALL 0
#define IDL4_NEED_MALLOC 1
#define IDL4_API x0
#define IDL4_ARCH ia32

#include "idl4/idl4.h"

#if IDL4_HEADER_REVISION < 20030403
#error You are using outdated versions of the IDL4 header files
#endif /* IDL4_HEADER_REVISION < 20030403 */

/* Interface simple */

void simple_server(void);
void simple_discard(void);

#define SIMPLE_DEFAULT_VTABLE { service_simple_foo }
#define SIMPLE_DEFAULT_VTABLE_SIZE 1
#define SIMPLE_MAX_FID 0
#define SIMPLE_MSGBUF_SIZE 9
#define SIMPLE_STRBUF_SIZE 0
#define SIMPLE_RCV_DOPE 0x12000
#define SIMPLE_FID_MASK 0

typedef union {
  struct {
    int _esp;
    int _ebp;
    char _spacer1[20];
    int __fid;
  } _in;
  struct {
    char _spacer1[52];
    int __eid;
  } _out;
} _param_simple_foo;

void service_simple_foo(int w1, int w0, l4_threadid_t _caller, _param_simple_foo _par) __attribute__ ((noreturn)) __attribute__ ((regparm (3)));

inline void simple_foo_implementation(CORBA_Object _caller, const CORBA_long parameter, idl4_server_environment *_env);

// Channel 0:                        ID SIZE ALGN FLAGS PRIO CIDX COFS SIDX SOFS
//   D0: l4_fpage_t _rcv_window       3    4    4 c----    0    0    0   -2   -8
//   D1: idl4_msgdope_t _size_dope    4    4    4 c----    0    1    4   -1   -4
//   D2: idl4_msgdope_t _send_dope    5    4    4 c----    0    2    8    0    0
//   D-: int _esp                     9    4    4 -s--p    0   -1   -1   -2   -8
//   D-: int _ebp                     A    4    4 -s--p    0   -1   -1   -1   -4
//   D-: l4_threadid_t _caller        B    4    4 ----p    0   -1   -1   -1   -1
//   R0: CORBA_long parameter         2    4    4 c-fx-    5    0   12    0   12
//   R2: int __fid                    0    4    4 csfxp    9    2   20    2   20
// 
//   Send dope: 0x6000                     Size dope: 0x6000
// 
// Channel 1:                        ID SIZE ALGN FLAGS PRIO CIDX COFS SIDX SOFS
//   D0: l4_fpage_t _rcv_window       6    4    4 c----    0    0    0   -2   16
//   D1: idl4_msgdope_t _size_dope    7    4    4 c----    0    1    4   -1   20
//   D2: idl4_msgdope_t _send_dope    8    4    4 c----    0    2    8    0   24
//   R2: int __eid                    0    4    4 csfxp    9    2   20    2   44
// 
//   Send dope: 0x6000
// 
// Client buffer:                |
//   IN:  ------------2222----0000
//   OUT: --------------------0000
// 
// Server buffer:                |
//   IN:  --------------------0000
//   OUT: --------------------------------------------0000

#define IDL4_PUBLISH_SIMPLE_FOO(_func) void service_simple_foo(int w1, int w0, l4_threadid_t _caller, _param_simple_foo _par)\
\
{ \
  idl4_server_environment _env; \
\
  _env._action = 0;\
\
  /* invoke service */ \
   \
  _func(_caller, (CORBA_long)w0, &_env);\
\
  if (IDL4_EXPECT_TRUE(_env._action == 0)) \
    { \
      /* jump back */ \
       \
      __asm__ __volatile__( \
        "lea -4+%0, %%esp                   \n\t" \
        "ret                                \n\t" \
        : \
        : "m" (_par), "a" (0), "S" (_caller), "D" ((unsigned int)_env._action)\
      ); \
    } \
\
  /* do not reply */ \
   \
  __asm__ __volatile__( \
    "lea -4+%0, %%esp                   \n\t" \
    "movl $-1, %%eax                    \n\t" \
    "ret                                \n\t" \
    : \
    : "m" (_par)\
  ); \
\
  /* prevent gcc from generating return code */ \
   \
  while (1); \
} \

#define IDL4_PUBLISH_simple_foo IDL4_PUBLISH_SIMPLE_FOO

static inline void simple_foo_reply(CORBA_Object _client)

{
  int _dummy;

  /* send message */
  
  __asm__ __volatile__(
    "push %%ebp                         \n\t"
    "mov $-1, %%ebp                     \n\t"
    "mov $0xf0, %%ecx                   \n\t"
    "xor %%eax, %%eax                   \n\t"
    "int $0x30                          \n\t"
    "pop %%ebp                          \n\t"
    : "=D" (_dummy), "=a" (_dummy), "=S" (_dummy)
    : "D" ((unsigned int)0), "S" (_client)
    : "cc", "memory", "ecx", "edx", "ebx"
  );
}

#endif /* !defined(__simple_server_h__) */
