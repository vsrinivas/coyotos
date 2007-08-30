#ifndef FQNAME_HXX
#define FQNAME_HXX
/*
 * Copyright (C) 2005, The EROS Group, LLC.
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


#include <stdlib.h>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <string>

#define FQ_NOIF ""

struct FQName {
  std::string iface;
  std::string ident;

  bool operator <(const FQName&) const;
  bool operator >(const FQName&) const;
  bool operator ==(const FQName&) const;
  FQName& operator =(const FQName&);

  bool operator !=(const FQName& _fqname) const 
  {
    return !(*this == _fqname);
  }

  bool isInitialized() 
  {
    return (ident.size() != 0);
  }
  
  FQName() 
  {
  }
//   FQName(const std::string& id) 
//   {
//     iface="__bogus";
//     ident = id;
//   }

  FQName(const FQName& that)
  {
    iface = that.iface;
    ident = that.ident;
  }

  FQName(const std::string& _iface, const std::string& _ident)
  {
    iface = _iface;
    ident = _ident;
  }

  std::string asString(const char *s = ".") const;
  std::string asString(const char *s, 
		       const std::string altifn) const;
};

inline
std::ostream& operator<<(std::ostream& strm, const FQName& fqn)
{
  strm << fqn.asString();
  return strm;
}

#endif /* FQNAME_HXX */
