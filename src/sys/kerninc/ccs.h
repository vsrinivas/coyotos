#ifndef __KERNINC_CCS_H__
#define __KERNINC_CCS_H__
/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

#include <inttypes.h>
#include <coyotos/machine/pagesize.h>

/**
 * @file ccs.h
 *
 * @brief Compilation system specific macros.
 *
 * Macro definitions for defining and declaring things that are
 * specific to a given C Compilation system, notably per-CPU, aligned,
 * and section-placed variables.
 */

#ifndef __GNUC__
#error "No current provision for any tool chain other than GCC"
#endif

/**
 * @brief wrapper macro for inline assembler
 *
 * This is a disgusting hack. The problem is that the GCC __asm__
 * syntax uses commas, and the preprocessor mistakes these for
 * multiple arguments. We really want a wrapper macro here so that we
 * can disable these lines for non-GCC compilers, so I am exploiting
 * (abusing?) the C99 variadic macro mechanism in an unusual way.
 */
#define GNU_INLINE_ASM(...) __asm__ __volatile__(__VA_ARGS__)

#define MY_CPU(nm) __cpu_private_##nm

#if MAX_NCPU > 1
#define DEFINE_CPU_PRIVATE(ty, nm) \
  __attribute__((__section__(".data.percpu"))) __typeof__(ty) MY_CPU(nm)

#else /* MAX_CPU==1 */

/// @brief Macro used to introduce a per-cpu variable.
#define DEFINE_CPU_PRIVATE(ty, nm) __typeof__(ty) MY_CPU(nm)

#endif

#define DECLARE_CPU_PRIVATE(ty, nm) extern __typeof__(ty) MY_CPU(nm)


#define CACHE_ALIGN  __attribute__((__aligned__(CACHE_LINE_SIZE), \
                                    __section__(".data.cachealign")))

#define PAGE_ALIGN  __attribute__((__aligned__(COYOTOS_PAGE_SIZE), \
                                     __section__(".pagedata")))

#define NORETURN __attribute__((noreturn))

#define alignof(x) __alignof__(x)

#endif /* __KERNINC_CCS_H__ */
