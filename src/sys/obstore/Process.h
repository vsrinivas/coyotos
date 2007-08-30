#ifndef __OBSTORE_PROCESS_H__
#define __OBSTORE_PROCESS_H__
/*
 * Copyright (C) 2006, Jonathan S. Shapiro.
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
 * @brief Externalization of process state. */

#include <obstore/capability.h>
#include <coyotos/machine/registers.h>

enum { CAP_REG_INDEX_BITS = 5,
       NUM_CAP_REGS = (1u << CAP_REG_INDEX_BITS) };

/* Bit layout of Process Flags Word:
 *
 *  31              15 14 13 12 11 10 9  8  7        0  
 * +------------------+--+--+--+--+--+--+--+----------+
 * |    reserved      |PC|CS|TR|TC|SI|SX|XM| runState |
 * +------------------+--+--+--+--+--+--+--+----------+
 *
 * Note that runState is implemented as a separate bit-field.
 */

#define PFLAGS_XM  0x01
#define PFLAGS_SX  0x02
#define PFLAGS_SI  0x04
#define PFLAGS_TC  0x08
#define PFLAGS_TR  0x10
#define PFLAGS_CS  0x20
#define PFLAGS_PC  0x40

/* Process run-states */
#define PRS_RUNNING   0
#define PRS_RECEIVING 1
#define PRS_FAULTED   2

/** @brief Externalized process state. This version is used internally
 * by the kernel.
 *
 * There is a very sleazy trick going on here.
 *
 * This structure is used by the kernel and the object store in
 * target-dependent form. In that form, the structure includes the
 * target-dependent register set.
 *
 * This structurs is used by mkimage and by the kernel in target @em
 * neutral form. In this form the target-dependent register set is not
 * present, and the structure has uniform size on all architectures.
 */
typedef struct ExProcess {
  uint8_t           runState;
  /* 8-bit hole available */
  uint16_t          flags;
  uint32_t          softInts;

  /** @brief Current fault code, or zero. */
  uint32_t          faultCode;

#if 0
  uint32_t          nParam;
#else
  uint32_t          /* PAD */: 32;
#endif

  /** @brief Auxiliary fault information (or zero). */
  coyaddr_t         faultInfo;

  /** @brief Schedule capability. */
  capability        schedule;	/* Type schedule or null */
  /** @brief Address space. */
  capability        addrSpace;	/* Type mem cap or null */
  /** @brief Brand capability. */
  capability        brand;	/* any */
  /** @brief Cohort capability. */
  capability        cohort;	/* any */
  /** @brief IO Space access */
  capability        ioSpace;	/* Type page */
  /** @brief Fault handler capability. */
  capability        handler;	/* Type sendFCRB */
  /** @brief Capability registers page capability. */
  capability        capReg[NUM_CAP_REGS]; /* Type capage.rw */

#if defined(COYOTOS_TARGET)
#if 0
  ActSaveArea_t     *actSaveArea; /* USER-mode pointer! */

  uintptr_t         scratchPad[128/sizeof(uintptr_t)];
  uintptr_t         rcvParam[16];
#endif

  fixregs_t         fixregs;
  floatregs_t       floatregs;
#ifdef COYOTOS_TARGET_HAS_SOFTREGS
  softregs_t        softregs;
#endif /* COYOTOS_TARGET_HAS_SOFTREGS */

#endif /* defined(COYOTOS_TARGET) */
} ExProcess;

#define OBSTORE_EXPROCESS_COMMON_SIZE \
  (offsetof(ExProcess, capReg) + sizeof(((ExProcess *)0)->capReg))
#endif /* __OBSTORE_PROCESS_H__ */
