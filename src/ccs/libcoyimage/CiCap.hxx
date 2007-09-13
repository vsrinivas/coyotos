#ifndef LIBCOYIMAGE_CICAP_HXX
#define LIBCOYIMAGE_CICAP_HXX
/*
 * Copyright (C) 2007, The EROS Group, LLC
 * All rights reserved.
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
 * @brief Definition of the in-memory structure corresponding to a
 * Coyotos image file.
 */

#include <stdlib.h>
#include <obstore/capability.h>
#include <obstore/ObFrameType.h>
#include <libsherpa/oBinaryStream.hxx>
#include <libsherpa/iBinaryStream.hxx>

const std::string& cap_typeName(const capability& cap);
const std::string& cap_typeName(uint32_t capType);

const ObFrameType cap_frameType(const capability& cap);
const ObFrameType cap_frameType(uint32_t capType);

bool cap_isValidType(uint32_t capType);

/** @brief Is capability a memory object or Window capability? */
inline bool
cap_isMemory(const capability& cap)
{
  switch(cap.type) {
  case ct_GPT:
  case ct_Page:
  case ct_CapPage:
  case ct_Window:
  case ct_LocalWindow:
    return true;
  default:
    return false;
  }
}

/** @brief Is capability a Window capability? */
inline bool
cap_isWindow(const capability& cap)
{
  switch(cap.type) {
  case ct_Window:
  case ct_LocalWindow:
    return true;
  default:
    return false;
  }
}

inline bool
cap_isType(const capability& cap, CapType ct)
{
  return (cap.type == (uint32_t) ct);
}

inline bool
cap_isMiscCap(const capability& cap)
{
  return (cap.type <= 0x20);
}

inline bool
cap_isObjectCap(const capability& cap)
{
  switch(cap.type) {
  case ct_Endpoint:
  case ct_Page:
  case ct_CapPage:
  case ct_GPT:
  case ct_Process:
    return true;
  default:
    return false;
  }
}

inline bool
cap_isEntryCap(const capability& cap)
{
  switch(cap.type) {
  case ct_Entry:
  case ct_AppNotice:
    return true;
  default:
    return false;
  }
}

inline void
cap_init(capability& cap)
{
  memset(&cap, 0, sizeof(cap));
}

inline coyaddr_t
cap_guard(const capability& cap)
{
  if (cap_isMemory(cap))
    return (coyaddr_t)cap.u1.mem.match << cap.u1.mem.l2g;

  return 0;
}

inline sherpa::oBinaryStream&
operator<<(sherpa::oBinaryStream& obs, const capability& cap)
{
  uint32_t *pCap32 = (uint32_t *) &cap;
  uint64_t *pCap64 = (uint64_t *) &cap;

  // First two words of cap go out as words:
  obs << pCap32[0];
  obs << pCap32[1];

  // Second two words of cap go out as doubleword:
  obs << pCap64[1];

  return obs;
}

inline sherpa::iBinaryStream&
operator>>(sherpa::iBinaryStream& ibs, capability& cap)
{
  uint32_t *pCap32 = (uint32_t *) &cap;
  uint64_t *pCap64 = (uint64_t *) &cap;

  // First two words of cap come in as words:
  ibs >> pCap32[0];
  ibs >> pCap32[1];

  // Second two words of cap come in as doubleword:
  ibs >> pCap64[1];

  return ibs;
}

std::ostream&
operator<<(std::ostream& strm, const capability& cap);

#endif /* LIBCOYIMAGE_CICAP_HXX */
