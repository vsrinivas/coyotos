#ifndef LIBCOYIMAGE_CIENDPOINT_HXX
#define LIBCOYIMAGE_CIENDPOINT_HXX
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

// Wrapper structures for the C versions published by the kernel:
struct CiEndpoint : public sherpa::Countable {
  bankid_t bank;		// not serialized
  oid_t    oid;			// not serialized

  ExEndpoint v;

  CiEndpoint()
  {
    bank = 0;
    oid = 0;
    memset(&v, 0, sizeof(v));
  }
};

inline sherpa::oBinaryStream&
operator<<(sherpa::oBinaryStream& obs, const CiEndpoint& ep)
{
  obs << ((uint32_t *) &ep.v)[0];	// id, bl, pm
  obs << ep.v.protPayload;
  obs << ep.v.endpointID;

  obs << ep.v.recipient;
  obs << ep.v.rcvQueue;

  return obs;
}

inline sherpa::iBinaryStream&
operator>>(sherpa::iBinaryStream& ibs, CiEndpoint& ep)
{
  ibs >> ((uint32_t *) &ep.v)[0];	// id, bl, pm

  ibs >> ep.v.protPayload;
  ibs >> ep.v.endpointID;

  ibs >> ep.v.recipient;
  ibs >> ep.v.rcvQueue;

  return ibs;
}

inline std::ostream&
operator <<(std::ostream& strm, CiEndpoint& ep)
{
  strm << "{ ENDP [bank=" << ep.bank << "]\n"
       << "       oid=" << ep.oid << '\n'
       << "       pm=" << ep.v.pm 
       << " sq=" << ep.v.sq
       << " pp=" << ep.v.protPayload
       << '\n'
       << "       epid=" << ep.v.endpointID << '\n' 
       << "       recip=" << ep.v.recipient << '\n'
       << "       rcvq=" << ep.v.rcvQueue << '\n'
       << "}\n";

  return strm;
}

#endif /* LIBCOYIMAGE_CIENDPOINT_HXX */
