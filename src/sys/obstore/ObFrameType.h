#ifndef __OBSTORE_OBFRAMETYPE_H__
#define __OBSTORE_OBFRAMETYPE_H__
/*
 * Copyright (C) 2006, Jonathan S. Shapiro.
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
 * @brief Frame type numbers at obstore interface */

/// @brief Enumeration of object types.
///
/// This must match the enumaration in range.idl
enum ObFrameType {
  oftPage = 0,
  oftCapPage = 1,
  oftGPT = 2,
  oftProcess = 3,
  oftEndpoint = 4,

  oftNUM_TYPES = 5,
  oftInvalid = -1,
};

#endif /* __OBSTORE_OBFRAMETYPE_H__ */
