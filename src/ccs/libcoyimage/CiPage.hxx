#ifndef LIBCOYIMAGE_CIPAGE_HXX
#define LIBCOYIMAGE_CIPAGE_HXX
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

#include <ctype.h>

// Wrapper structures for the C versions published by the kernel:
struct CiPage : public sherpa::Countable {
  bankid_t bank;		// not serialized
  oid_t    oid;			// not serialized
  size_t pgSize;		// not serialized

  uint8_t *data;

  CiPage(size_t pageSize)
  {
    bank = 0;
    oid = 0;
    pgSize = pageSize;
    data = new uint8_t[pgSize];
    memset(data, 0, pgSize);
  }

  ~CiPage()
  {
    delete [] data;
  }
};

//
// It would be really nice to only write out page data if the page was
// non-zero.  Unfortunately, this interacts badly with the
// serialization code.  For now, it's #ifdefed out.
//
inline sherpa::oBinaryStream&
operator<<(sherpa::oBinaryStream& obs, const CiPage& page)
{
#ifdef ZERO_PAGES_ELISION
  uint8_t hasData = false;
  for (size_t off = 0; off < page.pgSize; off++) {
    if (page.data[off] != 0) {
      hasData = true;
      break;
    }
  }
  obs << hasData;
  if (hasData)
#endif
    obs.write(page.pgSize, page.data);

  return obs;
}

inline sherpa::iBinaryStream&
operator>>(sherpa::iBinaryStream& ibs, CiPage& page)
{
#ifdef ZERO_PAGES_ELISION
  uint8_t hasData;

  ibs >> hasData;
  if (!hasData) {
    memset(page.data, 0, page.pgSize);
  } else
#endif
    ibs.read(page.pgSize, page.data);

  return ibs;
}

inline std::ostream&
operator <<(std::ostream& strm, CiPage& page)
{
  std::streamsize wid = strm.width();
  std::ios::fmtflags a = strm.flags(std::ios::hex);
  char fill = strm.fill(' ');

  uint8_t hasData = false;
  for (size_t off = 0; off < page.pgSize; off++) {
    if (page.data[off] != 0) {
      hasData = true;
      break;
    }
  }
  strm << "{ Page [bank="  << page.bank << "]\n"
       << "       oid=" << page.oid << '\n';

  size_t width = 0;
  size_t pgsize = page.pgSize - 1;
  while (pgsize > 0) {
    width++;
    pgsize /= 16;
  }
  strm << "        " << std::setw(width) << std::setfill(' ') << "" << " ";
  for (int i = 0; i < 16; i++) {
    strm << ' ' << i;
    if (i == 7)
      strm << ' ';
  }
  strm << " ";
  for (int i = 0; i < 16; i++)
    strm << i;
  strm << std::endl;
  
  bool dup = false;
  
  for (size_t offset = 0; offset < page.pgSize; offset += 16) {
    if (offset != 0 && offset + 16 < page.pgSize) {
      size_t i;
      for (i = 0; i < 16; i++)
	if (page.data[offset + i] != page.data[offset + i - 16])
	  break;
      if (i == 16) {
	if (!dup)
	  strm << "       " << std::setw(width) << std::setfill('*') << ""
	       << ": (repeats)" << std::endl;
	dup = true;
	continue;
      }
    }
    dup = false;
    strm << "       "
	 << std::setw(width) << std::setfill(' ') << offset
	 << ": "
	 << std::setfill('0');
    for (size_t i = 0; i < 16; i++) {
      strm << std::setw(2) << (unsigned)page.data[offset + i];
      if (i == 7)
	strm << ' ';
    }
    strm << ' ';
    for (size_t i = 0; i < 16; i++) {
      char c = page.data[offset + i];
      if (!isprint(c) && c != ' ')
	c = '.';
      strm << c;
    }
    strm << std::endl;
  }
  
  strm << "}" << std::endl;

  strm.flags(a);
  strm.fill(fill);
  strm.width(wid);

  return strm;
}

#endif /* LIBCOYIMAGE_CIPAGE_HXX */
