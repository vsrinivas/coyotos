#ifndef __IA32_EFLAGS_H__
#define __IA32_EFLAGS_H__
/*
 * Copyright (C) 2005, Jonathan S. Shapiro.
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
 * @brief flags register definitions.
 *
 */

#define EFLAGS_CF      0x00000001 /**< @brief carry flag */
/* Bit 1 is always forced to 1 */
#define EFLAGS_PF      0x00000004 /**< @brief parity flag */
/* Bit 3 is always forced to 0 */
#define EFLAGS_AF      0x00000010 /**< @brief aux carry flag */
/* Bit 5 is always forced to 0 */
#define EFLAGS_ZF      0x00000040 /**< @brief zero flag */
#define EFLAGS_SF      0x00000080 /**< @brief sign flag */
#define EFLAGS_TF      0x00000100 /**< @brief trap flag */
#define EFLAGS_IF      0x00000200 /**< @brief interrupt flag */
#define EFLAGS_DF      0x00000400 /**< @brief direction flag */
#define EFLAGS_OF      0x00000800 /**< @brief overflow flag */
#define EFLAGS_IOPL    0x00003000 /**< @brief IOPL mask */

/* Pre-shifted comparators: */
#define   EFLAGS_IOPL0 0x00000000 /**< @brief IOPL level 0 */
#define   EFLAGS_IOPL1 0x00001000 /**< @brief IOPL level 1 */
#define   EFLAGS_IOPL2 0x00002000 /**< @brief IOPL level 2 */
#define   EFLAGS_IOPL3 0x00003000 /**< @brief IOPL level 3 */

#define EFLAGS_NT      0x00004000 /**< @brief nested task */
#define EFLAGS_RF      0x00010000 /**< @brief resume flag */
#define EFLAGS_VM      0x00020000 /**< @brief virtual-8086 mode */
#define EFLAGS_AC      0x00040000 /**< @brief alignment check */
#define EFLAGS_VIF     0x00080000 /**< @brief virtual interrupt flag */
#define EFLAGS_VIP     0x00100000 /**< @brief virt. int. pending */
#define EFLAGS_ID      0x00200000 /**< @brief CPUID supported */

#endif /* __IA32_EFLAGS_H__ */
