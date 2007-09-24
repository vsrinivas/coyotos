#ifndef __KERNINC_PRINTF_H__
#define __KERNINC_KPRINTF_H__
/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief printf and friends.
 */

#include <kerninc/ccs.h>
#include <hal/machine.h>
#include <hal/config.h>

#if HAVE_HUI
extern void console_putc(const char);
extern void console_detach();
#ifdef BRING_UP
extern void console_shrink();
#endif
extern void printf(const char *, ...);
/** @brief Issue diagnostic and halt. 
 *
 * This should be used when the error is something that we plan to
 * fix.
 */
extern void bug(const char *, ...) NORETURN;
/** @brief Issue diagnostic and halt. 
 *
 * This should be used when the error is truly fatal, and is a valid
 * cause for a kernel stop.
 */
extern void fatal(const char *, ...) NORETURN;
#else /* !HAVE_HUI */
static inline void printf(const char *fmt, ...)
{
}
static inline void fatal(const char *fmt, ...)
{
  halt();
}
static inline void bug(const char *fmt, ...)
{
  halt();
}

#define printf(fmt, ...) ((void) 0)
#define fatal(fmt, ...)  halt()
#define bug(fmt, ...)  halt()
#endif

#endif /* __KERNINC_PRINTF_H__ */
