#ifndef __CAPIDL_H__
#define __CAPIDL_H__
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

#ifndef __ASSEMBLER__
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif
#include <coyotos/syscall.h>

#ifndef __ASSEMBLER__
typedef uint64_t errcode_t;

#define CAPIDL_U8(x)  x##u
#define CAPIDL_U16(x) x##u
#define CAPIDL_U32(x) x##ul
#define CAPIDL_U64(x) x##ull

#define CAPIDL_I8(x)  x
#define CAPIDL_I16(x) x
#define CAPIDL_I32(x) x##l
#define CAPIDL_I64(x) x##ll

typedef struct {
  caploc_t   replyCap;		/* IN: cap to send in reply slot */
  errcode_t  errCode;		/* OUT: exception code, if result not OK */
  uint64_t   epID;		/* IN/OUT: endpoint ID */
  uint32_t   pp;		/* OUT: protected payload */
} IDL_Environment;

/** @bug eventually, this needs to be thread-local */
extern IDL_Environment *__IDL_Env;

#define IDL_exceptCode (__IDL_Env->errCode)

#else /* __ASSEMBLER__ */

#define CAPIDL_U8(x)  x
#define CAPIDL_U16(x) x
#define CAPIDL_U32(x) x
#define CAPIDL_U64(x) x

#define CAPIDL_I8(x)  x
#define CAPIDL_I16(x) x
#define CAPIDL_I32(x) x
#define CAPIDL_I64(x) x

#endif /* __ASSEMBLER__ */

#define IDL_ALIGN_TO(x, y) (((x) + (y) - 1) & -(y))

#endif  /* __CAPIDL_H__ */
