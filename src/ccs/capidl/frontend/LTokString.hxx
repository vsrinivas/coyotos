#ifndef LSTRING_HXX
#define LSTRING_HXX

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

#include <libsherpa/LexLoc.hxx>
#include <libsherpa/LToken.hxx>
#include <libsherpa/CVector.hxx>

class LTokString {
  sherpa::CVector<sherpa::LToken> s;

  LTokString(const sherpa::CVector<sherpa::LToken>& s)
  {
    this->s = s;
  }

public:
  LTokString()
  {
  }

  LTokString(const LTokString& that)
  {
    s = that.s;
  }

  LTokString(const sherpa::LToken& tok)
    : s()
  {
    s.append(tok);
  }

  LTokString& operator=(const LTokString& that)
  {
    s = that.s;
    return *this;
  }

  LTokString& operator=(const sherpa::LToken& tok)
  {
    s.erase();
    s.append(tok);
    return *this;
  }

  LTokString operator+(const sherpa::LToken& tok)
  {
    return LTokString(s+tok);
  }

  LTokString& operator+=(const sherpa::LToken& tok)
  {
    s.append(tok);
    return *this;
  }

  LTokString operator+(const LTokString& tstr)
  {
    return LTokString(s+tstr.s);
  }

  LTokString& operator+=(const LTokString& tstr)
  {
    s.append(tstr.s);
    return *this;
  }

  std::string asEncodedString() const;

  std::string asString() const;

  inline size_t size() const
  {
    return s.size();
  }

  inline void erase()
  {
    s.erase();
  }
};

inline LTokString
operator+(const sherpa::LToken& tok,
	  const LTokString& tstr)
{
  return LTokString(tok) + tstr;
}

/* Adding two LTokens yields an LTokString: */
inline LTokString
operator+(const sherpa::LToken& tok,
	  const sherpa::LToken& tok2)
{
  return LTokString(tok) + tok2;
}

std::string DecodePositionalString(std::string s);

#endif /* LSTRING_HXX */
