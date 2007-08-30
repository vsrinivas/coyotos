/*
 * Copyright (C) 2005, Jonathan S. Shapiro.
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
 * @brief Supporting data structures for ctype.h.
 */

#include <inttypes.h>

uint8_t __isprint[16] = { /* a bitmask! */
  0x00,			/* BEL ACK ENQ EOT ETX STX SOH NUL */
  0x00,			/* SI  SO  CR  FF  VT  LF  TAB BS */
  0x00,			/* ETB SYN NAK DC4 DC3 DC2 DC1 DLE */
  0x00,			/* US  RS  GS  FS  ESC SUB EM  CAN */
  0xff,			/* '   &   %   $   #   "   !   SPC */
  0xff,			/* /   .   -   ,   +   *   )   ( */
  0xff,			/* 7   6   5   4   3   2   1   0 */
  0xff,			/* ?   >   =   <   ;   :   9   8 */
  0xff,			/* G   F   E   D   C   B   A   @ */
  0xff,			/* O   N   M   L   K   J   I   H */
  0xff,			/* W   V   U   T   S   R   Q   P */
  0xff,			/* _   ^   ]   \   [   Z   Y   X */
  0xff,			/* g   f   e   d   c   b   a   ` */
  0xff,			/* o   n   m   l   k   j   i   h */
  0xff,			/* w   v   u   t   s   r   q   p */
  0x7f,			/* DEL ~   }   |   {   z   y   x */
} ;
