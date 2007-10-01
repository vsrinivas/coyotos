#ifndef __KERNINC_MALLOC_H__
#define __KERNINC_MALLOC_H__
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

#include <stddef.h>
#include <hal/kerntypes.h>

/** @file 
 * @brief Definition of the heap interface, including malloc, free.
 *
 * While this header is called malloc.h for reasons of tradition, the
 * corresponding kernel implementation is found in kern_heap.c
 */

/** @brief Called from machine-dependent code to set up initial heap
    base, bound, and hard limit. */
extern void heap_init(kva_t start, kva_t backedTo, kva_t limit);

/** @brief Called from machine-dependent code to revise heap hard
 * limit (upwards).
 *
 * This is needed because on some architectures it is necessary to do
 * a small number of heap allocations before sizing the number of
 * processes and reserving space for page tables and such.
 */
extern void heap_adjust_limit(kva_t limit);

// kpa_t heap_AcquirePages(size_t nPage);

/** @brief Allocate @p nElem elements, each of size @p nBytes, aligned
    at a pointer boundary. */
void *calloc(size_t nBytes, size_t nElem);

#define CALLOC(ty,n)  ((ty *) calloc(sizeof(ty), (n)))
#define MALLOC(ty)    ((ty *) calloc(sizeof(ty), 1))

#endif /* __KERNINC_MALLOC_H__ */
