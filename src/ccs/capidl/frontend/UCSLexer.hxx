#ifndef UCSLEXER_HXX
#define UCSLEXER_HXX

/*
 * Copyright (C) 2007, The EROS Group, LLC.
 * All rights reserved.
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

typedef long ucs4_t;

#include <libsherpa/Path.hxx>
#include <libsherpa/LexLoc.hxx>
#include <iostream>

class UCSLexer {
  std::istream& inStream;
  std::ostream& errStream;

  static const int maxPutback = 3;
  ucs4_t putbackChars[maxPutback];
  int nputbackChars;

protected:
  std::string thisToken;

  int nDigits;
  int radix;

  bool debug;

  int num_errors;

  sherpa::LexLoc here;

  /// @brief Given a ucs4_t character, return its value as a decimal digit.
  long digitValue(ucs4_t);

  ucs4_t getChar();
  void   ungetChar(ucs4_t c);

  bool   slurpCComment();
  bool   slurpCxxComment();
  bool   slurpCxxDocComment();

  UCSLexer(std::istream& _in, sherpa::GCPtr<sherpa::Path> inputPath);

public:
  static bool validIdentPunct(uint32_t ucs4);
  static bool validIdentStart(uint32_t ucs4);
  static bool validIdentContinue(uint32_t ucs4);

#if 0
  bool validateIdentifier();
#endif
  bool validateString();

  void ReportParseError();
  void ReportParseError(const sherpa::LexLoc& loc, const std::string& msg);
  inline void ReportParseError(const std::string& msg) {
    ReportParseError(here, msg);
  }
  void ReportParseWarning(const sherpa::LexLoc& loc, const std::string& msg);
  inline void ReportParseWarning(const std::string& msg) {
    ReportParseWarning(here, msg);
  }

  inline void setDebug(bool showlex)
  {
    debug = (showlex ? true : false);
  }

  int NumErrors() { return (num_errors); }
};

#endif /* UCSLEXER_HXX */
