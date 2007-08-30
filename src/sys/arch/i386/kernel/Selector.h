#ifndef __SELECTOR_H__
#define __SELECTOR_H__
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
 *
 * @brief Segment numbers and selector values.
 */

/* THIS HEADER IS LOADED FROM ASSEMBLY CODE */

/// @brief NULL segment number
#define seg_Null         0x0
/// @brief TSS segment number
#define seg_TSS          0x1
/// @brief Kernel code segment number
#define seg_KernelCode   0x2
/// @brief Kernel data segment number
#define seg_KernelData   0x3
/// @brief Application code segment number
#define seg_AppCode      0x4
/// @brief Application data segment number
#define seg_AppData      0x5
/// @brief Application thread local storage segment number
#define seg_AppTLS       0x6

/// @brief Number of GDT entries
#define gseg_NGDT        0x7

#define SELECTOR(seg,rpl) ((seg*8)+rpl)

/// @brief Selector value for NULL segment
#define sel_Null       SELECTOR(seg_Null,0)
/// @brief Selector value for TSS segment
#define sel_TSS        SELECTOR(seg_TSS,0)
/// @brief Selector value for kernel code segment
#define sel_KernelCode SELECTOR(seg_KernelCode,0)
/// @brief Selector value for kernel data segment
#define sel_KernelData SELECTOR(seg_KernelData,0)
/// @brief Selector value for application code segment
#define sel_AppCode    SELECTOR(seg_AppCode,3)
/// @brief Selector value for application data segment
#define sel_AppData    SELECTOR(seg_AppData,3)
/// @brief Selector value for application thread local storage segment
#define sel_AppTLS     SELECTOR(seg_AppTLS,3)

#endif /* __SELECTOR_H__ */
