#ifndef __COYOTOS_SYSCALL_H__
#define __COYOTOS_SYSCALL_H__
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

#include <coyotos/machine/target.h>

/** @file 
 * @brief Declarations supporting the system call interface. */

#define sc_InvokeCap 0
                  /* 1 reserved */
#define sc_CopyCap   2
#define sc_Yield     3
                  /* 4..15 reserved */

/* Bit layout of IPW0 for sc_InvokeCap:
 *
 *  !         22 21 20 19 18 17 16 15 14 13 12 11 10  9 8   7 6   4 3  0 
 * +---------------------+--+--+--+--+--+--+--+--+-----+-----+-----+----+
 * | reserved(0)|EX|CO|AC|SC|RC|SP|RP|CW|NB|AS|SG| lrc | lsc | ldw |nr=0|
 * +---------------------+--+--+--+--+--+--+--+--+-----+-----+-----+----+
 *
 *  Where:
 *    nr   = system call number
 *    ldw  = last data word [0..7, inclusive]
 *    lsc  = last sent cap
 *    lrc  = last received cap
 *    SG   = transmit gather (1 => yes)
 *    AS   = accept scatter (1 => yes)
 *    NB   = non blocking (1 => yes)
 *    CW   = closed wait (1 => yes)
 *    SP   = send phase (1 => yes)
 *    RP   = receive phase (1 => yes)
 *    RC   = reply cap (1 => yes)
 *    SC   = send caps (1 => yes)
 *    AC   = accept caps (1 => yes)
 *    CO   = copy out soft regs (1 => yes) [IA-32 only]
 *    EX   = exceptional return (1 => yes) [IA-32 only]
 *
 * Reminder: ldw=0 is insufficient to signal send phase completion,
 * because field is reused on output to indicate number of transmitted
 * words.  In addition, with the addition of the EX bit to indicate 
 * exceptional return, there is no need for any payload for a simple
 * "Success" response.
 *
 *  On 32-bit platforms:
 *    Parameters On Input                    On Output
 *    PW[1..7]   data parameter words        data parameter words
 *    PW[8]      invoked cap                 rcv pp
 *    PW[9..12]  caploc_t for sent caps      NONE
 *    PW[13..16] caploc_t for received caps  NONE
 *    PW[17]     snd string len              snd string len (from sender)
 *    PW[18]     rcv string bound            NONE
 *    PW[19]     snd string ptr              NONE  
 *    PW[20]     rcv string ptr              NONE
 *    PW[21..22] matched epID                rcv epID
 *
 *  On 64-bit platforms:
 *    Parameters On Input                    On Output
 *    PW[1..7]   data parameter words        data parameter words
 *    PW[8]      invoked cap                 rcv pp
 *    PW[9..12]  caploc_t for sent caps      NONE
 *    PW[13..16] caploc_t for received caps  NONE
 *    PW[17]     snd string len              snd string len (from sender)
 *    PW[18]     rcv string bound            NONE
 *    PW[18]     snd string ptr              NONE
 *    PW[20]     rcv string ptr              NONE
 *    PW[21]     matched epID                rcv epID
 *
 * Bit layout of IPW0 for sc_CopyCap:
 *
 *  !                                               4 3  0  
 * +-------------------------------------------------+----+
 * |                  reserved (0)                   |nr=2|
 * +-------------------------------------------------+----+
 *
 *    IPW1 holds source caploc_t, IPW2 hold destination caploc_t.
 *
 * Bit layout of IPW0 for sc_Yield:
 *
 *  !                                               4 3  0  
 * +-------------------------------------------------+----+
 * |                  reserved (0)                   |nr=3|
 * +-------------------------------------------------+----+
 *
 */

#define IPW0_NR_MASK   0x000000f
#define IPW0_NR(ipw0) ((ipw0) & IPW0_NR_MASK)
#define IPW0_MAKE_NR(nr) ((nr) & 0xf)
#define IPW0_WITH_NR(ipw0, nr) (((ipw0) & ~IPW0_NR_MASK) | IPW0_MAKE_NR(nr))

#define IPW0_LDW_MASK  0x0000070
#define IPW0_LDW(ipw0) (((ipw0) & IPW0_LDW_MASK) >> 4)
#define IPW0_MAKE_LDW(ldw) (((ldw) & 0x7) << 4)
#define IPW0_WITH_LDW(ipw0,ldw) (((ipw0) & ~IPW0_LDW_MASK) | IPW0_MAKE_LDW(ldw))

#define IPW0_LSC_MASK  0x0000180
#define IPW0_LSC(ipw0) (((ipw0) & IPW0_LSC_MASK) >> 7)
#define IPW0_MAKE_LSC(lsc) (((lsc) & 0x3) << 7)
#define IPW0_WITH_LSC(ipw0,lsc) (((ipw0) & ~IPW0_LSC_MASK) | IPW0_MAKE_LSC(lsc))

#define IPW0_LRC_MASK  0x0000600
#define IPW0_LRC(ipw0) (((ipw0) & IPW0_LRC_MASK) >> 9)
#define IPW0_MAKE_LRC(lrc) (((lrc) & 0x3) << 9)
#define IPW0_WITH_LRC(ipw0,lrc) (((ipw0) & ~IPW0_LRC_MASK) | IPW0_MAKE_LRC(lrc))

#define IPW0_SG        0x0000800
#define IPW0_AS        0x0001000
#define IPW0_NB        0x0002000
#define IPW0_CW        0x0004000
#define IPW0_RP        0x0008000
#define IPW0_SP        0x0010000
#define IPW0_RC        0x0020000
#define IPW0_SC        0x0040000
#define IPW0_AC        0x0080000
#define IPW0_CO        0x0100000
#define IPW0_EX        0x0200000

/* Input control bits that are preserved on completion of receive phase */
#define IPW0_PRESERVE (IPW0_LRC_MASK|IPW0_AS|IPW0_AC|IPW0_CO|IPW0_EX)

#define IPW_ICW          0
#define IPW_DW0          0
#define IPW_OPCODE       IPW_DW0+1

#define IPW_INVCAP       8
#define OPW_PP           8
#define IPW_SNDCAP       9
#define IPW_RCVCAP       13
#define IPW_SNDLEN       17
#define OPW_SNDLEN       17
#define IPW_RCVBOUND     18
#define IPW_SNDPTR       19
#define IPW_RCVPTR       20
#define IPW_EPID         21
#define OPW_EPID         21

#define COYOTOS_MAX_SNDLEN 65536

#if defined(COYOTOS_TARGET)

#if (COYOTOS_HW_ADDRESS_BITS == 32)
#define COYOTOS_PARAMETER_WORDS 23
#elif (COYOTOS_HW_ADDRESS_BITS == 64)
#define COYOTOS_PARAMETER_WORDS 22
#endif

#endif /* defined(COYOTOS_TARGET) */

#if !defined(__ASSEMBLER__)

#include <inttypes.h>
#include <coyotos/coytypes.h>

typedef struct {
  uintptr_t pw[8];
  union {
    caploc_t  invCap;		/* on entry */
    uintptr_t pp;		/* on exit */
  } u;
  caploc_t  sndCap[4];
  caploc_t  rcvCap[4];
  uint32_t  sndLen;		/* FIX: Should be 16 bits */
  uint32_t  rcvBound;		/* FIX: Should be 16 bits */
  void     *sndPtr;
  void     *rcvPtr;
  uint64_t  epID;
} InvParameterBlock_t;

typedef struct {
  uintptr_t icw;
  uint64_t  exceptionCode;
} InvExceptionParameterBlock_t;

#if !defined(__KERNEL__)

#include <stdbool.h>
#include <assert.h>

static inline bool invoke_capability(InvParameterBlock_t *);
static inline void cap_copy(caploc_t dest, caploc_t source);
static inline void yield(void);

/** @brief Set up @p pb for an error reply with @p errCode.  
 *
 * Only effects the SC, LDW, and EX bits of IPW0, as well as IPW1 and
 * possibly IPW2.
 */
static inline void invoke_setErrorReply(InvParameterBlock_t *pb, 
					uint64_t errCode)
{
  InvExceptionParameterBlock_t *epb = (InvExceptionParameterBlock_t *) pb;
  epb->icw = IPW0_MAKE_LDW((sizeof(*epb)/sizeof(uintptr_t))-1) | IPW0_EX;
  epb->exceptionCode = errCode;
}

static inline uint64_t invoke_errCode(InvParameterBlock_t *pb)
{
  InvExceptionParameterBlock_t *epb = (InvExceptionParameterBlock_t *) pb;
  return epb->exceptionCode;
}

#endif /* !defined(__KERNEL__) */
#endif /* !defined(__ASSEMBLER__) */

#include <coyotos/machine/syscall.h>

#endif /* __COYOTOS_SYSCALL_H__ */
