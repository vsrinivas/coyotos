#ifndef __I386_ASM_H__
#define __I386_ASM_H__
/*
 * Copyright (C) 2005, The EROS Group, LLC.
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
 * @brief Assembly directive macros.
 */
#define ALIGN 16

#ifndef __ELF__
#error Do not know how to compile for non-ELF target.
#endif

#define EXT(x)  x
#define LEXT(x) x## :
#define GEXT(x) .globl EXT(x); LEXT(x)

#define	ALIGNEDVAR(x,al) .globl EXT(x); .align al; LEXT(x)
#define	VAR(x)		ALIGNEDVAR(x,4)
#define	ENTRY(x)	.globl EXT(x); .type EXT(x),@function; LEXT(x)
#define	GDATA(x)	.globl EXT(x); .align ALIGN; LEXT(x)

#endif /* __I386_ASM_H__ */
