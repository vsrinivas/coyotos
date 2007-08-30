#ifndef INOSTREAM_HXX
#define INOSTREAM_HXX
/*
 * Copyright (C) 2006, The EROS Group, LLC.
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
#include <errno.h>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <string>
#include <assert.h>
#include <sstream>

using namespace sherpa;
using namespace std;

// This is a completely sleazy way of making an automatically indenting 
// stream.
struct INOstream {
  size_t depth;
  size_t col;
  bool needIndent;
  std::ostream &ostrm;
  //void doIndent();

  INOstream(std::ostream &os) 
    :ostrm(os)
  {
    depth = 0;
    col = 0;
    needIndent = true;
  }

  inline void indent(int i)
  {
    depth += i;
  }

  size_t indent_for_macro()
  {
    size_t odepth = depth;
    depth = 0;
    return odepth;
  }

  inline int indentToHere()
  {
    size_t old_depth = depth;
    depth = (depth > col) ? depth : col;
    return old_depth;
  }
  inline void setIndent(size_t theCol)
  {
    depth = theCol;
  }

  inline void more()
  {
    indent(2);
  }

  inline void less()
  {
    assert(depth >= 2);
    indent(-2);
  }

  void
  doIndent()
  {
    //std::cout << "Indent = " << depth << "; needIndent = " << needIndent << endl;
    if (needIndent) {
      while (col < depth) {
	ostrm << ' ';
	col++;
      }
    }
    needIndent = false;
  }

  inline
  INOstream& operator<<(const char c)
  {
    INOstream& inostrm = *this;
    inostrm.doIndent();

    inostrm.ostrm << c;
    inostrm.col++;

    if (c == '\n') {
      inostrm.col = 0;
      inostrm.needIndent = true;
    }
    return inostrm;
  }

  inline
  INOstream& operator<<(const unsigned char c)
  {
    INOstream& inostrm = *this;
    inostrm.doIndent();

    inostrm.ostrm << c;
    inostrm.col++;
    if (c == '\n') {
      inostrm.col = 0;
      inostrm.needIndent = true;
    }
    return inostrm;
  }

  // This is where the magic happens. The way that a C++ stream recognizes
  // things like std::endl is by overloading this argument type. std::endl 
  // is actually a procedure!
  //
  // Note, however, that INOstream does not forward the endl. We need
  // it in order to recognize when a fill is required, but we don't
  // really intend to flush the underlying output stream.
  inline
  INOstream& operator<<(ostream& (*pf)(ostream&))
  {
    INOstream& inostrm = *this;
    if (pf == (ostream& (*)(ostream&)) std::endl) {
      inostrm << '\n';
    }
    else {
      inostrm.doIndent();
      inostrm.ostrm << pf;
    }
    return inostrm;
  }

  inline
  INOstream& operator<<(ios_base& (*pf)(ios_base&))
  {
    INOstream& inostrm = *this;
    inostrm.doIndent();
    inostrm.ostrm << pf;
    return inostrm;
  }

  inline
  INOstream& operator<<(const char *s)
  {
    INOstream& inostrm = *this;

    for (size_t i = 0; i < strlen(s); i++)
      inostrm << s[i];

    return inostrm;
  }

#if 0
  inline
  INOstream& operator<<(const unsigned long long ull)
  {
    char digits[64];		// sufficient in all cases
    sprintf(digits, "%ull", ull);

    INOstream& inostrm = *this;
    inostrm << digits;

    return inostrm;
  }

  inline
  INOstream& operator<<(const long long ll)
  {
    char digits[64];		// sufficient in all cases
    sprintf(digits, "%ll", ll);

    INOstream& inostrm = *this;
    inostrm << digits;

    return inostrm;
  }

  inline
  INOstream& operator<<(const unsigned long ul)
  {
    char digits[64];		// sufficient in all cases
    sprintf(digits, "%ul", ul);

    INOstream& inostrm = *this;
    inostrm << digits;

    return inostrm;
  }

  inline
  INOstream& operator<<(const long l)
  {
    char digits[64];		// sufficient in all cases
    sprintf(digits, "%l", l);

    INOstream& inostrm = *this;
    inostrm << digits;

    return inostrm;
  }

  INOstream& operator<<(const unsigned int ui)
  {
    char digits[64];		// sufficient in all cases
    sprintf(digits, "%ul", ui);

    INOstream& inostrm = *this;
    inostrm << digits;

    return inostrm;
  }

  inline
  INOstream& operator<<(const int i)
  {
    char digits[64];		// sufficient in all cases
    sprintf(digits, "%l", i);

    INOstream& inostrm = *this;
    inostrm << digits;

    return inostrm;
  }
#endif

  inline
  INOstream& operator<<(const std::string& s)
  {
    INOstream& inostrm = *this;

    for (size_t i = 0; i < s.size(); i++)
      inostrm << s[i];

    return inostrm;
  }

  template<typename T>
  inline
  INOstream& operator<<(T ob)
  {
    INOstream& inostrm = *this;
    inostrm.ostrm << ob;
    return inostrm;
  }

};

#endif /* INOSTREAM_HXX */
