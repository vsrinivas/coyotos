#ifndef __COYTYPES_H__
#define __COYTYPES_H__
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

#include <stdint.h>
#include <stdbool.h>

#include <coyotos/machine/pagesize.h>

typedef uint64_t  oid_t;
typedef uint64_t  coyaddr_t;
typedef uint32_t  guard_t;
typedef coyaddr_t archaddr_t;

/** @brief Defined the number of bits in a software address */
#define COYOTOS_SOFTADDR_BITS  64

/** @brief Defines the size of a capability in a CapPage */
#define COYOTOS_CAPABILITY_SIZE 16

static inline uint8_t guard_l2g(guard_t g)
{
  return (g & 0xff);
}

static inline uint32_t guard_match(guard_t g)
{
  return (g >> 8);
}

static inline uint64_t guard_mask(guard_t g)
{
  if (guard_l2g(g) >= COYOTOS_SOFTADDR_BITS)
    return (0);
  return (~0ull) << guard_l2g(g);
}

static inline uint64_t guard_matchValue(guard_t g)
{
  if (guard_l2g(g) >= COYOTOS_SOFTADDR_BITS)
    return (0);
  return (uint64_t)guard_match(g) << guard_l2g(g);
}

static inline bool guard_matches(guard_t g, uint64_t addr)
{
  return (addr & guard_mask(g)) == guard_matchValue(g);
}

static inline guard_t make_guard(uint32_t match, uint32_t l2g)
{
  return (match << 8) | l2g;
}

// typedef uint64_t  ipcword_t;

/* Type field definitions for capitem_t */
#define CAPLOC_TY_REG  0x0
#define CAPLOC_TY_MEM  0x1

#if 0
typedef uint32_t capitem32_t;
typedef uint64_t capitem64_t;
#endif

typedef union caploc32_t {
  struct {
    uint32_t  ty  : 1;
    uint32_t  loc : 31;
  } fld;
  uint32_t raw;
} caploc32_t;

typedef union caploc64_t {
  struct {
    uint64_t  ty  : 1;
    uint64_t  loc : 63;
  } fld;
  uint64_t raw;
} caploc64_t;

typedef struct stringitem32_t {
  /** @brief Either 0 (non-continued) or 1 (continued) */
  uint32_t  type : 4;
  uint32_t  zero : 7;
  /** @brief Index (&lt;=4M) of last byte transmitted. */ 
  uint32_t  len  : 21;

  /** @brief Location of string */
  uint32_t  ptr;
} stringitem32_t;

typedef struct stringitem64_t {
  /** @brief Either 0 (non-continued) or 1 (continued) */
  uint64_t  type : 4;
  uint64_t  zero : 7;
  /** @brief Index (&lt;=4M) of last byte transmitted. */
  uint64_t  len  : 21;

  /** @brief Location of string */
  uint64_t  ptr;
} stringitem64_t;

#if defined(COYOTOS_ARCH)
#if (COYOTOS_HW_ADDRESS_BITS == 32)

typedef caploc32_t caploc_t;
typedef stringitem32_t stringitem_t;

#elif (COYOTOS_HW_ADDRESS_BITS == 64)

typedef caploc64_t caploc_t;
typedef stringitem64_t stringitem_t;

#else
#error "Invalid value for COYOTOS_HW_ADDRESS_BITS"
#endif

#define ADDR_CAPLOC(a) \
  ((caploc_t) { .fld.ty = CAPLOC_TY_MEM, .fld.loc = ((uintptr_t)(a) >> 1) })
#define REG_CAPLOC(t) ((caploc_t) { .fld.ty = CAPLOC_TY_REG, .fld.loc = (t) })
#define REG_CAPREG(t) ((capreg_t) { .fld.ty = CAPLOC_TY_REG, .fld.loc = (t) })

#endif /* defined(COYOTOS_ARCH) */

#endif  /* __COYTYPES_H__ */
