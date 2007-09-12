#ifndef __IA32_FPU_H__
#define __IA32_FPU_H__
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
 *
 * @brief Helper structure declarations to deal with floating point
 * save.
 */

typedef uint32_t sse_reg[4];
typedef uint16_t extended_reg[5];

// Following for documentation purposes only.
struct fxsave64 {
  uint16_t     fcw;
  uint16_t     fsw;
  uint16_t     ftw;
  uint16_t     fop;
  uint64_t     rip;	// CS:IP if operand size modifier is 32-bit
  uint64_t     rdp;	// DS:DP if operand size modifier is 32-bit
  uint32_t     mxcsr;
  uint32_t     mxcsr_mask;
  sse_reg      fpr[8];	// a.k.a. mmx
  sse_reg      xmm[16];
};

// Following for re-arranging the save area when saving on legacy
// processors.
struct fnsave {
  uint32_t     f_ctrl;
  uint32_t     f_status;
  uint32_t     f_tag;
  uint32_t     f_ip;
  uint16_t     f_cs;
  uint16_t     f_opcode;
  uint32_t     f_dp;
  uint32_t     f_ds;
  extended_reg f_reg[8];
};


#endif /* __IA32_FPU_H__ */
