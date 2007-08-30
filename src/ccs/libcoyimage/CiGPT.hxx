#ifndef LIBCOYIMAGE_CIGPT_HXX
#define LIBCOYIMAGE_CIGPT_HXX
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

#include <iomanip>

struct CiGPT : public sherpa::Countable {
  bankid_t bank;		// not serialized
  oid_t    oid;			// not serialized

  ExGPT v;

  CiGPT(size_t l2pgSize)
  {
    bank = 0;
    oid = 0;
    memset(&v, 0, sizeof(v));
    v.l2v = l2pgSize;
  }

  size_t
  l2slots()
  {
    if (v.ha || v.bg)
      return GPT_SLOT_INDEX_BITS - 1;

    return GPT_SLOT_INDEX_BITS;
  }
};

inline sherpa::oBinaryStream&
operator<<(sherpa::oBinaryStream& obs, const CiGPT& gpt)
{
  obs << ((uint32_t *) &gpt.v)[0];	// l2v, ha, bg
  obs << (uint32_t) 0;		// PAD

  for (size_t i = 0; i < NUM_GPT_SLOTS; i++)
    obs << gpt.v.cap[i];
  return obs;
}

inline sherpa::iBinaryStream&
operator>>(sherpa::iBinaryStream& ibs, CiGPT& gpt)
{
  uint32_t u;
  ibs >> ((uint32_t *) &gpt.v)[0];	// l2v, ha, bg
  ibs >> u;			// PAD

  for (size_t i = 0; i < NUM_GPT_SLOTS; i++)
    ibs >> gpt.v.cap[i];
  return ibs;
}

inline std::ostream&
operator <<(std::ostream& strm, CiGPT& gpt)
{
  std::ios_base::fmtflags flgs = strm.flags(std::ios_base::fmtflags(0));

  strm << "{ GPT [bank=" << gpt.bank << "]\n"
       << "      oid=" << gpt.oid << '\n';
  for (size_t i = 0; i < NUM_GPT_SLOTS; i++)
    strm << "      cap["
	 << std::setw(2) << std::setfill('0') << i
	 << "]=" << gpt.v.cap[i] << '\n';
  strm << "}\n";

  strm.flags(flgs);

  return strm;
}

#endif /* LIBCOYIMAGE_CIGPT_HXX */
