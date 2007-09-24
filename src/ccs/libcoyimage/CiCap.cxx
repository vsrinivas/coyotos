/*
 * Copyright (C) 2007, The EROS Group, LLC.
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

#include <iostream>

#include <libsherpa/UExcept.hxx>

#include <obstore/capability.h>
#include <obstore/ObFrameType.h>

#include "CiCap.hxx"

using namespace sherpa;

static const std::string capTypeNames[] = {
#define DEFCAP(ty, ft, code)   #ty,
#define NODEFCAP(ty, ft, code) "type=" #code,
#include <obstore/captype.def>
};

static const bool capTypeValid[] = {
#define DEFCAP(ty, ft, code)   1,
#define NODEFCAP(ty, ft, code) 0,
#include <obstore/captype.def>
};

static ObFrameType capFrameTypes[] = {
#define DEFCAP(ty, ft, code)   oft##ft,
#define NODEFCAP(ty, ft, code) oft##ft,
#include <obstore/captype.def>
};

bool
cap_isValidType(uint32_t capType)
{
  if (capType >= (sizeof (capTypeValid) / sizeof (capTypeValid[0])))
    return false;
  return capTypeValid[capType];
}

const std::string&
cap_typeName(const capability& cap)
{
  return cap_typeName(cap.type);
}

const std::string&
cap_typeName(uint32_t capType)
{
  if (capType >= (sizeof(capTypeNames) / sizeof(capTypeNames[0])))
    THROW(excpt::Assert, "capType value exceeds representable bound");

  return capTypeNames[capType];
}

const ObFrameType
cap_frameType(const capability& cap)
{
  return cap_frameType(cap.type);
}

const ObFrameType
cap_frameType(uint32_t capType)
{
  if (capType >= (sizeof(capFrameTypes) / sizeof(capFrameTypes[0])))
    THROW(excpt::Assert, "capType value exceeds representable bound");

  return capFrameTypes[capType];
}

std::ostream&
operator<<(std::ostream& strm, const capability& cap)
{
  strm << "{";

  if (cap.swizzled)
    strm << " P";
  if (cap.restr & CAP_RESTR_RO)
    strm << " RO";
  if (cap.restr & CAP_RESTR_NX)
    strm << " NX";
  if (cap.restr & CAP_RESTR_WK)
    strm << " WK";
  if (cap.type == ct_Page) {
    if (cap.restr & CAP_RESTR_CD)
      strm << " CD";
    if (cap.restr & CAP_RESTR_WT)
      strm << " WT";
  }
  else {
    if (cap.restr & CAP_RESTR_OP)
      strm << " OP";
    if (cap.restr & CAP_RESTR_NC)
      strm << " NC";
  }
  strm << " " << cap_typeName(cap) << "(ty=" << cap.type << ")";

  std::ios_base::fmtflags flgs = strm.flags(std::ios_base::fmtflags(0));

  strm << std::showbase;

  /* The following if/else chains *should* be disjoint */
  if (cap_isObjectCap(cap) || cap_isEntryCap(cap)) {
    strm << " ac=" << cap.allocCount;
    strm << std::hex << " oid=" << cap.u2.oid << std::dec;
  }
  else if (cap_isWindow(cap))
    strm << std::hex << " offset=" << cap.u2.offset << std::dec;


  /* The following if/else chains *should* be disjoint */
  if (cap_isEntryCap(cap))
    strm << " pp=" << cap.u1.protPayload;

  else if (cap_isMemory(cap))
    strm << std::hex << " match=" << cap.u1.mem.match << std::dec
	 << " l2g=" << cap.u1.mem.l2g;

  // Other misc caps have no further content
  strm << " }";

  flgs = strm.flags(flgs);

  return strm;
}
