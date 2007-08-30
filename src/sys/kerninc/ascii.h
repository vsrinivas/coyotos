#ifndef __KERNINC_ASCII_H__
#define __KERNINC_ASCII_H__
/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the EROS Operating System runtime library.
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
 * @brief printf and friends.
 */

/** @brief definitions of ascii character codes. */
enum {
  ASCII_NUL = 0x00,
  ASCII_SOH = 0x01,
  ASCII_STX = 0x02,
  ASCII_ETX = 0x03,
  ASCII_EOT = 0x04,
  ASCII_ENQ = 0x05,
  ASCII_ACK = 0x06,
  ASCII_BEL = 0x07,
  ASCII_BS  = 0x08,
  ASCII_TAB = 0x09,
  ASCII_LF  = 0x0a,
  ASCII_VT  = 0x0b,
  ASCII_FF  = 0x0c,
  ASCII_CR  = 0x0d,
  ASCII_SO  = 0x0e,
  ASCII_SI  = 0x0f,
  ASCII_DLE = 0x10,
  ASCII_DC1 = 0x11,
  ASCII_DC2 = 0x12,
  ASCII_DC3 = 0x13,
  ASCII_DC4 = 0x14,
  ASCII_NAK = 0x15,
  ASCII_SYN = 0x16,
  ASCII_ETB = 0x17,
  ASCII_CAN = 0x18,
  ASCII_EM  = 0x19,
  ASCII_SUB = 0x1a,
  ASCII_ESC = 0x1b,
  ASCII_FS  = 0x1c,
  ASCII_GS  = 0x1d,
  ASCII_RS  = 0x1e,
  ASCII_US  = 0x1f,
  ASCII_SPC = 0x20,
  ASCII_DEL = 0x7f,
};

#endif /* __KERNINC_ASCII_H__ */
