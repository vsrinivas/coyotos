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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include <string>
#include <sstream>

#include "LTokString.hxx"

std::string 
LTokString::asEncodedString() const
{
  std::ostringstream msg;

  for (size_t i = 0; i < s.size(); i++)
    msg << s[i].asEncodedString();

  return msg.str();
}

std::string 
LTokString::asString() const
{
  std::ostringstream msg;

  for (size_t i = 0; i < s.size(); i++)
    msg << s[i].str;

  return msg.str();
}

std::string DecodePositionalString(std::string s)
{
  std::ostringstream out;
  for (size_t i = 0; i < s.length(); i++) {
    if (s[i] == '#') {
      if (s[i+1] == '{') {
	i = sherpa::LexLoc::skip(s, i) - 1;
      }
      else {
	assert(s[i+1] == '#');
	out << s[i];
	i++;
      }
    }
    else
      out << s[i];
  }

  return out.str();
}


