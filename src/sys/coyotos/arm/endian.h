#ifndef __ARM_ENDIAN_H__
#define __ARM_ENDIAN_H__
/*
 * Copyright (C) 2007, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define BYTE_ORDER_ARM    LITTLE_ENDIAN

#if (COYOTOS_TARGET == COYOTOS_TARGET_arm)
#define BYTE_ORDER        BYTE_ORDER_ARM
#endif

#if defined(__arm__)
#define TARGET_BYTE_ORDER BYTE_ORDER_ARM
#define BITFIELD_PACK_LOW
#endif

#endif  /* __ARM_ENDIAN_H__ */
