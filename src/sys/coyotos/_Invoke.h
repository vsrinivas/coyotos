#ifndef __INVOKE_H__
#define __INVOKE_H__

/*
 * Copyright (C) 2007, Jonathan S. Shapiro.
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

#include <stdint.h>

#if 0
/*
 * This file resides in eros/ because the kernel and the invocation
 * library must agree on the values.
 *
 * Changes to the Message structure must be reflected in the assembler
 * stubs in lib/domain/ARCH/...S
 */
#endif

/* Local Variables: */
/* comment-column:34 */
/* End: */

#if !defined(__ASSEMBLER__) /* && !defined(__KERNEL__) */

#include <coyotos/coytypes.h>
#include <coyotos/endian.h>

#ifndef BITFIELD_PACK_LOW
# error "Check bitfield packing"
#endif

#define N_PAYLOAD_CAP  4
#define N_PAYLOAD_WORD 4

typedef struct InvCtl_t {
  uint32_t NB :  1;
  uint32_t ST :  1;
  uint32_t RC :  1;
  uint32_t CW :  1;
  uint32_t YX :  1;
  uint32_t SS :  1;
  uint32_t SR :  1;
  uint32_t    : 25;
} InvCtl_t;

typedef struct InvParam32_t {
  InvCtl_t  invCtl;		  /* IN */
  uint32_t  rcvPP;		  /* OUT */
  uint64_t  rcvEpID;		  /* IN/OUT */
  capitem_t invCap;		  /* IN */
  capitem_t sndCap[N_PAYLOAD_CAP]; /* IN */
  ipcword_t sndWord[N_PAYLOAD_WORD]; /* IN */
  capitem_t rcvCap[N_PAYLOAD_CAP]; /* OUT */
  ipcword_t rcvWord[N_PAYLOAD_WORD]; /* OUT */
  uint32_t  sndLen;		  /* IN/OUT */
  uint32_t  sndPtr;		  /* IN */
  uint32_t  rcvBound;		  /* IN */
  uint32_t  rcvPtr;		  /* IN */
} InvParam32_t;

typedef struct InvParam64_t {
  InvCtl_t  invCtl;		  /* IN */
  uint32_t  rcvPP;		  /* OUT */
  uint64_t  rcvEpID;		  /* IN/OUT */
  capitem_t invCap;		  /* IN */
  capitem_t sndCap[N_PAYLOAD_CAP]; /* IN */
  ipcword_t sndWord[N_PAYLOAD_WORD]; /* IN */
  capitem_t rcvCap[N_PAYLOAD_CAP]; /* OUT */
  ipcword_t rcvWord[N_PAYLOAD_WORD]; /* OUT */
  uint32_t  sndLen;		  /* IN/OUT */
  uint64_t  sndPtr;		  /* IN */
  uint32_t  rcvBound;		  /* IN */
  uint64_t  rcvPtr;		  /* IN */
} InvParam64_t;

#ifdef __cplusplus
extern "C" {
#endif

  extern ipcword_t SendAndWait32(InvParam32_t *);
  extern ipcword_t SendAndWait64(InvParam64_t *);

#if (COYOTOS_ADDRESS_BITS == 32)
  typedef InvParam32_t InvParam_t;

  static inline ipcword_t SendAndWait(InvParam_t *iparam)
  {
    return SendAndWait32(iparam);
  }
#elif (COYOTOS_ADDRESS_BITS == 64)
  typedef InvParam64_t InvParam_t;

  static inline ipcword_t SendAndWait(InvParam_t *iparam)
  {
    return SendAndWait64(iparam);
  }
#else
#error "Invalid value for COYOTOS_ADDRESS_BITS"
#endif

#ifdef __cplusplus
}
#endif

#endif /* !ASSEMBLER */

/* Construction of capitem_t values: */
#define capitem_null      ((capitem_t) 0)
#define capitem_reg(x)    ((capitem_t) (((reg) << 4) | 0x1u)
#define capitem_addr(x)   ((capitem_t) (((a) & ~0xfu) | 0x2u)
#define capitem_datargn(x,sz) \
  ((capitem_t) (((x) & ~((1u << (sz)) - 1u)) | (((sz) - 1u) << 4) | 4u)
#define capitem_caprgn(x,sz) \
  ((capitem_t) (((x) & ~((1u << (sz)) - 1u)) | (((sz) - 1u) << 4) | 5u)
#define capitem_iorgn(x,sz) \
  ((capitem_t) (((x) & ~((1u << (sz)) - 1u)) | (((sz) - 1u) << 4) | 6u)

/* see ObRef introduction */ 
#define RC_OK   	    0u

#endif /* __INVOKE_H__ */
