#ifndef __ENDIAN_H__
#define __ENDIAN_H__
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

/** @file
 * @brief Support for byte-order definition and detection.
 *
 * While the Coyotos kernel proper should not have any byte order
 * dependencies in a given implementation, the definition of byte
 * order is architecture dependent and therefore properly supplied by
 * the kernel header.
 */
#define LITTLE_ENDIAN  1234
#define BIG_ENDIAN     4321
#define PDP_ENDIAN     3412	/* fond ancient history */

#include <coyotos/machine/endian.h>

#endif  /* __ENDIAN_H__ */
