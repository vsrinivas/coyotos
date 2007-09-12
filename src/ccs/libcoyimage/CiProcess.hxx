#ifndef LIBCOYIMAGE_CIPROCESS_HXX
#define LIBCOYIMAGE_CIPROCESS_HXX
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

#include <iomanip>

// Wrapper structures for the C versions published by the kernel:
struct CiProcess : public sherpa::Countable {
  bankid_t bank;		// not serialized
  oid_t    oid;			// not serialized

  ExProcess v;

  CiProcess()
  {
    bank = 0;
    oid = 0;
    memset(&v, 0, sizeof(v));
  }
};

inline sherpa::oBinaryStream&
operator<<(sherpa::oBinaryStream& obs, const CiProcess& proc)
{
  obs << proc.v.runState;
  obs << proc.v.flags;
  obs << proc.v.notices;
  obs << proc.v.faultCode;
  obs << (uint32_t) 0;		// PAD
  obs << proc.v.faultInfo;

  obs << proc.v.schedule;
  obs << proc.v.addrSpace;
  obs << proc.v.brand;
  obs << proc.v.cohort;
  obs << proc.v.ioSpace;
  obs << proc.v.handler;
  for (size_t i = 0; i < NUM_CAP_REGS; i++)
    obs << proc.v.capReg[i];

  return obs;
}

inline sherpa::iBinaryStream&
operator>>(sherpa::iBinaryStream& ibs, CiProcess& proc)
{
  uint32_t u;

  ibs >> proc.v.runState;
  ibs >> proc.v.flags;
  ibs >> proc.v.notices;
  ibs >> proc.v.faultCode;
  ibs >> u;			// PAD
  ibs >> proc.v.faultInfo;

  ibs >> proc.v.schedule;
  ibs >> proc.v.addrSpace;
  ibs >> proc.v.brand;
  ibs >> proc.v.cohort;
  ibs >> proc.v.ioSpace;
  ibs >> proc.v.handler;
  for (size_t i = 0; i < NUM_CAP_REGS; i++)
    ibs >> proc.v.capReg[i];

  return ibs;
}

inline std::ostream&
operator <<(std::ostream& strm, CiProcess& proc)
{
  std::ios_base::fmtflags flgs = strm.flags(std::ios_base::fmtflags(0));

  strm << "{ PROC [bank=" << proc.bank << "]\n"
       << "       oid=" << proc.oid << '\n'
       << "       runState=" << proc.v.runState << '\n'
       << "       flags=" << std::hex 
       << proc.v.flags << std::dec << '\n'
       << "       notices=" << std::hex 
       << proc.v.notices << std::dec << '\n'
       << "       fltCode=" << proc.v.faultCode
       << "       fltInfo=" << std::hex 
       << proc.v.faultInfo << std::dec << '\n'
       << "       sched=" << proc.v.schedule << '\n'
       << "       addrSpace=" << proc.v.addrSpace << '\n'
       << "       ioSpace=" << proc.v.ioSpace << '\n'
       << "       brand=" << proc.v.brand << '\n'
       << "       cohort=" << proc.v.cohort << '\n'
       << "       handler=" << proc.v.handler << '\n';

  for (size_t i = 0; i < NUM_CAP_REGS; i++)
    strm << "       capReg["
	 << std::setw(2) << std::setfill('0') << i
	 << "]=" << proc.v.capReg[i] << '\n';
  strm << "}\n";

  strm.flags(flgs);

  return strm;
}

#endif /* LIBCOYIMAGE_CIPROCESS_HXX */
