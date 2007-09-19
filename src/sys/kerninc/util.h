#ifndef __KERNINC_UTIL_H__
#define __KERNINC_UTIL_H__
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

/**
 * @file
 *
 * @brief Miscellaneous utility routines.
 */


/** @brief Align a value down to the next multiple of al. The value of
    al must be a power of two. */
#define align_down(v, al) ((v) & ~((al) - 1))
#define align_up(v, al)   align_down(((v) + (al) - 1), al)

/**
 * @brief Compute min(a, b) in a type-safe, single-evaluation form.  
 *
 * This must be a macro because it's type-indifferent.
 */
#define min(a, b) \
  ({ \
    __typeof(a) tmpa = (a); \
    __typeof(b) tmpb = (b); \
    (((tmpa) < (tmpb)) ? (tmpa) : (tmpb)); \
  })

/**
 * @brief Compute max(a, b) in a type-safe, single-evaluation form.  
 *
 * This must be a macro because it's type-indifferent.
 */
#define max(a, b) \
  ({ \
    __typeof(a) tmpa = (a); \
    __typeof(b) tmpb = (b); \
    (((tmpa) > (tmpb)) ? (tmpa) : (tmpb)); \
  })

/** @brief field extraction for unsigned fields. 
 *
 * Upper limit is INCLUSIVE.
 */
#define FIELD(word, hi, lo) \
  ( (((2u << (hi))-1u) & (word)) >> (lo))

/** @brief Return the bits above @p bits in @p addr, shifted to be at the
 * bottom of value.
 *
 * Checks for shift width exceeding word size.
 *
 * Precondition: 0 @< bits @<= COYOTOS_ADDRESS_BITS
 */
#define safe_right_shift(addr, n)			\
  ({ size_t _maxbits = sizeof(addr)*8;			\
    __typeof__(n) _n = (n);				\
    (_n < _maxbits ? ((addr) >> _n) : 0); })

#define safe_low_bits(addr, n)				\
  ({ size_t _maxbits = sizeof(addr)*8;			\
     __typeof__(n) _n = (n);				\
     typeof(addr) _mask = (_n < _maxbits ?		\
       ((typeof(addr))1 << _n) - 1 : -(typeof(addr))1);	\
     ((addr) & _mask); })

#endif /* __KERNINC_UTIL_H__ */
