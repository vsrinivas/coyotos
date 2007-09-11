#ifndef __KERNINC_CTYPE_H__
#define __KERNINC_CTYPE_H__
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

/** @file
 * @brief Character types
 */

extern uint8_t __isprint[16];

static inline int isspace(int c)
{
  return (c == ' ' || c == '\t');
}

static inline int isdigit(int c)
{
  return (c >= '0' && c <= '9');
}

static inline int isalpha(int c)
{
  return ((c >= 'a' && c <= 'z') ||
	  (c >= 'A' && c <= 'Z'));
}

static inline int toupper(int c)
{
  return ((c >= 'a' && c <= 'z') ? 
	  (c + 'A' - 'a') : c);
}

/* Cannot use ctype from libc */
static inline int isprint(int c)
{
  if (c > 127)
    return false;

  uint8_t mask = __isprint[(c >> 3)];
  c &= 0x7u;
  if (c)
    mask >>= c;
  if (mask & 1)
    return true;
  return false;
}

#endif /* __KERNINC_CTYPE_H__ */
