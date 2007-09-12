#ifndef __I386_PAGESIZE_H__
#define __I386_PAGESIZE_H__

/*
 * Copyright (C) 2006, The EROS Group, LLC.
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
 * @brief Target-specific page size definitions
 *
 * This file is included by assembly code, and it should therefore be
 * careful not to use constructs such as trailing constant modifiers
 * that would be incompatible with assembly language use of those
 * constants.
 *
 * This file also includes some macros that are derived from these
 * constants.
 */

/* *************************************************************
 *
 * Items defined here must be agreed on between kernel and driver
 * code.
 *
 *************************************************************** */

/** @brief Smallest page size supported by the kernel.
 *
 * Note that this page size may be larger than the smallest page size
 * supported by the target hardware architecture.
 */
#define COYOTOS_I386_PAGE_SIZE	0x1000
/** @brief Number of page offset bits. */
#define COYOTOS_I386_PAGE_ADDR_BITS	12
/** @brief Mask value used to extract or supppress page offset bits.
 *
 * This could be derived from PAGE_ADDR_BITS, but not within the
 * expression syntax supported by the assembler. */
#define COYOTOS_I386_PAGE_ADDR_MASK	0xfff

/** @brief Number of virtual address bits supported for this
    target. */
#define COYOTOS_I386_ADDRESS_BITS	32

#if (COYOTOS_TARGET == COYOTOS_TARGET_i386)

#define COYOTOS_PAGE_SIZE       COYOTOS_I386_PAGE_SIZE
#define COYOTOS_PAGE_ADDR_BITS  COYOTOS_I386_PAGE_ADDR_BITS
#define COYOTOS_PAGE_ADDR_MASK  COYOTOS_I386_PAGE_ADDR_MASK
#define COYOTOS_HW_ADDRESS_BITS COYOTOS_I386_ADDRESS_BITS

#endif

#endif /* __I386_PAGESIZE_H__ */
