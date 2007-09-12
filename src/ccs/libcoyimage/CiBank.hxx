#ifndef LIBCOYIMAGE_CIBANK_HXX
#define LIBCOYIMAGE_CIBANK_HXX
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

struct CiBank : public CoyImgBank {
  CiBank()
  {
    oid = 0;
    parent = 0;
  }

  CiBank(oid_t _oid, bankid_t _parent)
  {
    oid = _oid;
    parent = _parent;
  }
};

inline sherpa::oBinaryStream&
operator<<(sherpa::oBinaryStream& obs, const CiBank& bank)
{
  obs << bank.oid;
  obs << bank.parent;
  return obs;
}

inline sherpa::iBinaryStream&
operator>>(sherpa::iBinaryStream& ibs, CiBank& bank)
{
  ibs >> bank.oid;
  ibs >> bank.parent;
  return ibs;
}

#endif /* LIBCOYIMAGE_CIBANK_HXX */
