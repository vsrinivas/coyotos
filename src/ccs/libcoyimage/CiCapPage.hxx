#ifndef LIBCOYIMAGE_CICAPPAGE_HXX
#define LIBCOYIMAGE_CICAPPAGE_HXX
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
struct CiCapPage : public sherpa::Countable {
  bankid_t bank;		// not serialized
  oid_t    oid;			// not serialized
  size_t pgSize;		// not serialized

  capability *data;

  CiCapPage(size_t pageSize)
  {
    bank = 0;
    oid = 0;
    pgSize = pageSize;
    data = new capability[pgSize/sizeof(capability)];
    memset(data, 0, pgSize);
  }
};

//
// It would be really nice to only write out page data if the page was
// non-zero.  Unfortunately, this interacts badly with the
// serialization code.  For now, it's #ifdefed out.
//
inline sherpa::oBinaryStream&
operator<<(sherpa::oBinaryStream& obs, const CiCapPage& capPage)
{
#ifdef ZERO_PAGES_ELISION
  uint8_t hasData = false;

#if 0
  capability nullCap;
  cap_init(nullCap);
#endif

  for (size_t i = 0; i < capPage.pgSize/sizeof(capability); i++) {
    if (capPage.data[i].type != ct_Null) {
      hasData = true;
      break;
    }
#if 0 // possible test, to make sure we aren't zeroing a bad Null cap
    if (memcmp(&capPage.data[i], &nullCap, sizeof (nullCap)) != 0) {
      THROW(excpt::BadValue, "Null cap not identically zero");
    }
#endif
  }
  obs << hasData;
  if (hasData)
#endif
    for (size_t i = 0; i < capPage.pgSize/sizeof(capability); i++)
      obs << capPage.data[i];

  return obs;
}

inline sherpa::iBinaryStream&
operator>>(sherpa::iBinaryStream& ibs, CiCapPage& capPage)
{
#ifdef ZERO_PAGES_ELISION
  uint8_t hasData;
  capability nullCap;
  cap_init(nullCap);

  ibs >> hasData;
#endif
  
  for (size_t i = 0; i < capPage.pgSize/sizeof(capability); i++)
#ifdef ZERO_PAGES_ELISION
    if (!hasData)
      capPage.data[i] = nullCap;
    else
#endif
      ibs >> capPage.data[i];
  
  return ibs;
}

inline std::ostream&
operator <<(std::ostream& strm, CiCapPage& capPage)
{
#if 0
  std::streamsize wid = strm.width();
  std::ios::fmtflags a = strm.flags(std::ios::hex);
  char fill = strm.fill(' ');
#endif
  std::ios_base::fmtflags flgs = strm.flags(std::ios_base::fmtflags(0));

  strm << "{ CapPage [bank="  << capPage.bank << "]\n"
       << "          oid=" << capPage.oid << '\n';

  uint32_t nPrint = 0;
  size_t nSlots = capPage.pgSize / sizeof(capability);

  for (size_t i = 0; i < nSlots; i++) {
    if (capPage.data[i].type != ct_Null) {
      strm << "          ["
	   << std::setw(3) << std::setfill('0') << i
	   << "]=" << capPage.data[i] << '\n';
      nPrint++;
    }
  }
  if (nPrint == 0)
    strm << "        (all capabilities null)\n";

  strm << "}\n";
  strm.flags(flgs);

  return strm;
}

#endif /* LIBCOYIMAGE_CICAPPAGE_HXX */
