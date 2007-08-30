/*
 * Copyright (C) 2007, The EROS Group, LLC
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
 * @brief Physical memory-related functions
 */

#include <coyotos/coytypes.h>
#include <kerninc/pstring.h>
#include <kerninc/string.h>
#include <hal/transmap.h>
#include <kerninc/util.h>

void 
memcpy_vtop(kpa_t cur, void *s_arg, size_t length)
{
  uintptr_t s = (uintptr_t)s_arg;
  while (length > 0) {
    uintptr_t offset = cur & COYOTOS_PAGE_ADDR_MASK;
    char *map = TRANSMAP_MAP(cur - offset, char *);
    char *addr = map + offset;
    size_t count = min(length, COYOTOS_PAGE_SIZE - offset);
    memcpy(addr, (void *)s, count);
    TRANSMAP_UNMAP(map);
    cur += count;
    s += count;
    length -= count;
  }
}


void 
memcpy_ptov(void *d_arg, kpa_t cur, size_t length)
{
  uintptr_t d = (uintptr_t)d_arg;
  while (length > 0) {
    uintptr_t offset = cur & COYOTOS_PAGE_ADDR_MASK;
    char *map = TRANSMAP_MAP(cur - offset, char *);
    char *addr = map + offset;
    size_t count = min(length, COYOTOS_PAGE_SIZE - offset);
    memcpy((void *)d, addr, count);
    TRANSMAP_UNMAP(map);
    d += count;
    cur += count;
    length -= count;
  }
}

void 
memcpy_ptop(kpa_t cur, kpa_t s, size_t length)
{
  while (length > 0) {
    uintptr_t offset = cur & COYOTOS_PAGE_ADDR_MASK;
    char *map = TRANSMAP_MAP(cur - offset, char *);
    char *addr = map + offset;
    size_t count = min(length, COYOTOS_PAGE_SIZE - offset);
    memcpy_ptov(addr, s, count);
    TRANSMAP_UNMAP(map);
    cur += count;
    s += count;
    length -= count;
  }
}

#include <kerninc/printf.h>
void 
memset_p(kpa_t cur, int u, size_t length)
{
  while (length > 0) {
    uintptr_t offset = cur & COYOTOS_PAGE_ADDR_MASK;
    char *map = TRANSMAP_MAP(cur - offset, char *);
    char *addr = map + offset;
    size_t count = min(length, COYOTOS_PAGE_SIZE - offset);
    memset(addr, u, count);
    TRANSMAP_UNMAP(map);
    cur += count;
    length -= count;
  }
}

