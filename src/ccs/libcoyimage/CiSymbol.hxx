#ifndef LIBCOYIMAGE_CISYMBOL_HXX
#define LIBCOYIMAGE_CISYMBOL_HXX
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

struct Symbol {
  std::string name;
  capability  cap;

  Symbol()
  {
    cap_init(cap);
  }
};

inline sherpa::oBinaryStream&
operator<<(sherpa::oBinaryStream& obs, const Symbol& sym)
{
  obs << (uint32_t) sym.name.size();
  obs << sym.name;
  obs << sym.cap;
  return obs;
}


inline sherpa::iBinaryStream&
operator>>(sherpa::iBinaryStream& ibs, Symbol& sym)
{
  uint32_t sz;
  ibs >> sz;
  ibs.readString(sz, sym.name);
  ibs >> sym.cap;
  return ibs;
}

#endif /* LIBCOYIMAGE_CISYMBOL_HXX */
