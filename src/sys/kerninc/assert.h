#ifndef __KERNINC_ASSERT_H__
#define __KERNINC_ASSERT_H__
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
 *
 * @brief Kernel assert support.
 */

#include <kerninc/ccs.h>

/* Implemented in kern_kprintf.c: */
extern uint32_t __assert(unsigned line, const char *filename, 
			 const char *s) NORETURN;

#ifdef NDEBUG

#define assert(e)  ((void) 0)

#else /* !NDEBG */

#define assert(e)				\
  ((e)						\
   ? ((void) 0)					\
   : (__assert(__LINE__, __FILE__, #e), ((void) 0)))

#endif

#define require(e)				\
  ((e)						\
   ? ((void) 0)					\
   : (__assert(__LINE__, __FILE__, #e), ((void) 0)))

#endif /* __KERNINC_ASSERT_H__ */
