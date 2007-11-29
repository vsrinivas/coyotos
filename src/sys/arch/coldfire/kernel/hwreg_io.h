#ifndef __COLDFIRE_HWREG_IO_H__
#define __COLDFIRE_HWREG_IO_H__
/*
 * Copyright (C) 2007, The EROS Group, LLC.
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
 * @brief Declarations and procedures to manipulate hardware
 * registers.
 *
 * The declarations include the base addresses at which the various
 * functional units are located in the memory space.
 */


/* MMU registers occupy a 64-kbyte region beginning at MMUBAR */
#define COLDFIRE_MMUBAR 0x0
#define COLDFIRE_MMUCR  (COLDFIRE_MMUBAR + 0x00)
#define COLDFIRE_MMUOR  (COLDFIRE_MMUBAR + 0x04)
#define COLDFIRE_MMUSR  (COLDFIRE_MMUBAR + 0x08)
/* Reserved: 0x0c */
#define COLDFIRE_MMUAR  (COLDFIRE_MMUBAR + 0x10)
#define COLDFIRE_MMUTR  (COLDFIRE_MMUBAR + 0x14)
/*  MMUTR:
 *  31            10 9   2  1  0
 * +----------------+-----+--+--+
 * |  VA            |ASID |SG| V|
 * +----------------+-----+--+--+
 */
#define COLDFIRE_MMUDR  (COLDFIRE_MMUBAR + 0x18)

/* Bits the specify MMU operations. MC5485RM Section 5.5.3.4. */
#define COLDFIRE_MMUOR_STLB  0x100   /**< Search TLB  */
#define COLDFIRE_MMUOR_CA    0x080   /**< Clear all entries */
#define COLDFIRE_MMUOR_CNL   0x040   /**< Clear non-locked entries */
#define COLDFIRE_MMUOR_CAS   0x020 /**< Clear non-locked matching ASID */
#define COLDFIRE_MMUOR_ITLB  0x010 /**< ITLB operation */
#define COLDFIRE_MMUOR_ADR   0x008 /**< TLB address select */
#define COLDFIRE_MMUOR_RW    0x004 /**< Read or write operation */
#define COLDFIRE_MMUOR_ACC   0x002 /**< MMU TLB Access */
#define COLDFIRE_MMUOR_UAA   0x001 /**< Update allocation address */

#ifndef __ASSEMBLER__

/** @brief Read memory-mapped register. */
static inline uint32_t
hwreg_read(uint32_t regno)
{
  volatile uint32_t *regPtr = (uint32_t *) regno;

  return *regPtr;
}

static inline void
hwreg_write(uint32_t regno, uint32_t value)
{
  volatile uint32_t *regPtr = (uint32_t *) regno;

  *regPtr = value;
}

static inline void *
hwreg_read_ptr(uint32_t regno)
{
  return (void *) hwreg_read(regno);
}

static inline void
hwreg_write_ptr(uint32_t regno, void *vp)
{
  hwreg_write(regno, (uintptr_t) vp);
}

#endif /* __ASSEMBLER__ */

#endif /* __COLDFIRE_HWREG_IO_H__ */
