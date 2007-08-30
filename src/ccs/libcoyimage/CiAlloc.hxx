#ifndef LIBCOYIMAGE_CIALLOC_HXX
#define LIBCOYIMAGE_CIALLOC_HXX
/*
 * Copyright (C) 2007, Jonathan S. Shapiro.
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

#include <obstore/ObFrameType.h>
#include "CiBank.hxx"

struct CiAlloc : public CoyImgAlloc {
  CiAlloc()
  {
    fType = oftInvalid;
    oid = 0;
    bank = 0;
  }

  CiAlloc(ObFrameType _fType, oid_t _oid, bankid_t _bank)
  {
    fType = _fType;
    oid = _oid;
    bank = _bank;
  }

  static const size_t AllocEntrySize = 
    2 * sizeof (oid_t) + 2 * sizeof (uint32_t);
};

inline sherpa::oBinaryStream&
operator<<(sherpa::oBinaryStream& obs, const CiAlloc& alloc)
{
  obs << alloc.oid;
  obs << alloc.bank;
  obs << alloc.fType;
  obs << (uint32_t) 0;
  return obs;
}

inline sherpa::iBinaryStream&
operator>>(sherpa::iBinaryStream& ibs, CiAlloc& alloc)
{
  uint32_t u;

  ibs >> alloc.oid;
  ibs >> alloc.bank;
  ibs >> alloc.fType;
  ibs >> u; /* PAD */
  return ibs;
}

#endif /* LIBCOYIMAGE_CIALLOC_HXX */
