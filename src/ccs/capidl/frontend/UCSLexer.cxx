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

#include <assert.h>
#include <string>

#include <unicode/uchar.h>

#include <libsherpa/utf8.hxx>
#include "UCSLexer.hxx"

using namespace sherpa;

void
UCSLexer::ReportParseError()
{
  errStream << here
	    << ": syntax error (via yyerror)" << '\n';
  num_errors++;
}
 
void
UCSLexer::ReportParseError(const LexLoc& loc, const std::string& msg)
{
  errStream << loc
	    << ": "
	    << msg << std::endl;

  num_errors++;
}

void
UCSLexer::ReportParseWarning(const LexLoc& loc, const std::string& msg)
{
  errStream << loc
	    << ": "
	    << msg << std::endl;
}

long 
UCSLexer::digitValue(ucs4_t ucs4)
{
  long l = -1;

  if (ucs4 >= '0' && ucs4 <= '9')
    l = ucs4 - '0';
  if (ucs4 >= 'a' && ucs4 <= 'f')
    l = ucs4 - 'a' + 10;
  if (ucs4 >= 'A' && ucs4 <= 'F')
    l = ucs4 - 'A' + 10;

  if (l > radix)
    l = -1;
  return l;
}

bool
UCSLexer::validIdentPunct(uint32_t ucs4)
{
  if (strchr("_", ucs4))
    return true;
  return false;
}

bool
UCSLexer::validIdentStart(uint32_t ucs4)
{
  return (u_hasBinaryProperty(ucs4,UCHAR_XID_START) || 
	  validIdentPunct(ucs4));
}

bool
UCSLexer::validIdentContinue(uint32_t ucs4)
{
  return (u_hasBinaryProperty(ucs4,UCHAR_XID_CONTINUE) ||
	  validIdentPunct(ucs4));
}

#if 0
bool
UCSLexer::validateIdentifier()
{
  size_t pos;
  if (!validIdentStart(thisToken[0])) {
    ReportParseError(here,
		     std::string("The character ")
		     + thisToken[0] + " does start a valid token.\n");
    return false;
  }

  for (pos = 1; pos < thisToken.size(); pos++) {
    if (!validIdentContinue(thisToken[pos])) {
      ReportParseError(here,
		       std::string("The character ")
		       + thisToken[pos] + " is not valid in an identifier.\n");
      return false;
    }
  }

  return true;
}
#endif

bool
UCSLexer::validateString()
{
  const char *s = thisToken.c_str();
  const char *spos = s;
  uint32_t c;

  while (*spos) {
    const char *snext;
    c = sherpa::utf8_decode(spos, &snext);

    if (c == ' ') {		/* spaces are explicitly legal */
      spos = snext;
    }
    else if (c == '\\') {	/* escaped characters are legal */
      const char *escStart = spos;
      spos++;
      switch (*spos) {
      case 'n':
      case 't':
      case 'r':
      case 'b':
      case 's':
      case 'f':
      case '"':
      case '\\':
	{
	  spos++;
	  break;
	}
      case '{':
	{
	  if (*++spos != 'U')
	    return (spos - s);
	  if (*++spos != '+')
	    return (spos - s);
	  while (*++spos != '}')
	    if (!isxdigit(*spos))
	      return (spos - s);
	}
	spos++;
	break;
      default:
	{
	  LexLoc badHere = here;
	  badHere.offset += (escStart - s);
	  ReportParseError(badHere, 
			   "Illegal (non-printing) character in string '"
			   + thisToken + "'\n");
	  // Bad escape sequence
	  return false;
	}
      }
    }
    else if (u_isgraph(c)) {
      spos = snext;
    }
    else {
      LexLoc badHere = here;
      badHere.offset += (spos - s);
      ReportParseError(badHere, 
		       "Illegal (non-printing) character in string '"
		       + thisToken + "'\n");
      // Bad escape sequence
      return false;
    }
  }

  return -1;
}

ucs4_t
UCSLexer::getChar()
{
  char utf[8];
  unsigned char c;

  long ucs4;

  if (nputbackChars > 0) {
    assert(nputbackChars <= maxPutback);
    ucs4 = putbackChars[--nputbackChars];
    if (ucs4 == EOF)
      return EOF;		// previous ungetChar of EOF
    utf8_encode(ucs4, utf);
    goto checkDigit;
  }

  memset(utf, 0, 8);

  utf[0] = inStream.get();
  c = utf[0];
  if (utf[0] == EOF)
    return EOF;

  if (c <= 127)
    goto done;

  utf[1] = inStream.get();
  if (utf[1] == EOF)
    return EOF;

  if (c <= 223)
    goto done;

  utf[2] = inStream.get();
  if (utf[2] == EOF)
    return EOF;

  if (c <= 239)
    goto done;

  utf[3] = inStream.get();
  if (utf[3] == EOF)
    return EOF;

  if (c <= 247)
    goto done;
 
  utf[4] = inStream.get();
  if (utf[4] == EOF)
    return EOF;

  if (c <= 251)
    goto done;

  utf[5] = inStream.get();
  if (utf[5] == EOF)
    return EOF;

 done:
  ucs4 = utf8_decode(utf, 0);
 checkDigit:
  thisToken += utf;

  if (digitValue(ucs4) < radix)
    nDigits++;

  return ucs4;
}

void
UCSLexer::ungetChar(ucs4_t c)
{
  char utf[8];

  assert(nputbackChars < maxPutback);
  putbackChars[nputbackChars++] = c;

  if (c != EOF) {
    // This is not actually supposed to happen, but guard against it.
    unsigned len = utf8_encode(c, utf);
    thisToken.erase( thisToken.length() - len);
  }
}


UCSLexer::UCSLexer(std::istream& _in, GCPtr<Path> inputPath)
  :inStream(_in), errStream(std::cerr), here(inputPath, 1, 0)
{
  inStream.unsetf(std::ios_base::skipws);

  num_errors = 0;
  debug = false;
  nputbackChars = 0;
}

/// @brief Assuming we have already seen /*, grabs the remainder of a
/// C-style comment.
bool
UCSLexer::slurpCComment()
{
  ucs4_t c;

  while ((c = getChar()) != EOF) {
    if (c == '*') {
      ucs4_t c2 = getChar();
      if (c2 == '/') {
	here.updateWith(thisToken);
	return true;
      }

      ungetChar(c2);
    }
  }

  ReportParseError("comment ends in EOF");
  here.updateWith(thisToken);
  return false;
}

/// @brief Assuming we have already seen //, grabs the remainder of a
/// C++-style comment line.
bool
UCSLexer::slurpCxxComment()
{
  for (;;) {
    ucs4_t c = getChar();

    if (c == EOF) {
      here.updateWith(thisToken);
      ReportParseWarning("comment ends in EOF");
      return true;
    }

    if (c == '\r') {
      ucs4_t c2 = getChar();
      if (c2 != '\n' && c2 != EOF)	// \r\n is treated as \n
        ungetChar(c2);
      c = '\n';			// continue as if we got a '\n'
    }

    if (c == '\n') {
      here.updateWith(thisToken);
      return true;
    }
  }
}

/// @brief Assuming that we have already seen ///, grabs the remainder
/// of a (possibly multiline) C++-style documentation comment.
bool
UCSLexer::slurpCxxDocComment()
{
  bool atBOL = false;
  size_t line_start = 0;

  for (;;) {
    ucs4_t c = getChar();

    if (c == ' ' || c == '\t')
      continue;

    if (c == EOF) {
      here.updateWith(thisToken);
      ReportParseWarning("comment ends in EOF");
      return true;
    }

    if (c == '\r') {
      ucs4_t c2 = getChar();
      if (c2 != '\n' && c2 != EOF)	// \r\n is treated as \n
        ungetChar(c2);
      c = '\n';			// continue as if we got a '\n'
    }

    if (c == '\n') {
      if (atBOL) {
	// We have just processed a completely blank line, which
	// terminates the current C++ doc comment. Bring the 'here'
	// location up to date, but then truncate the trailing white
	// space that has been appended to thisToken.
	here.updateWith(thisToken);
	thisToken = thisToken.substr(0, line_start);

	return true;
      }

      // We are at the beginning of a line, but we need to "look
      // ahead" to see if this line may continue the documentation
      // comment. Record current end of token so that we can erase
      // later whitespace if the next line turns out NOT to continue
      // this comment:
      atBOL = true;
      line_start = thisToken.length();
    }
    else if (atBOL && c == '/') {
      // Look ahead two characters to see if this is the start of ///,
      // in which case this is a continuing documentation comment.

      ucs4_t c2 = getChar();
      ucs4_t c3 = getChar();

      if ((c2 != '/') || (c3 != '/')) {
	// Not a doc comment continuation. Back out the lookahead,
	// process the current token, and truncate out the trailing
	// white space.
	ungetChar(c3);
	ungetChar(c2);
	ungetChar(c);

	here.updateWith(thisToken);
	thisToken = thisToken.substr(0, line_start);

	return true;
      }

      /// Continuing the documentation comment, so turn atBOL off:
      atBOL = false;
      continue;
    }
    else if (atBOL) {
      /// This is the beginning of a non-comment token. Back out the
      /// current lookahead and return:
      ungetChar(c);

      here.updateWith(thisToken);
      thisToken = thisToken.substr(0, line_start);

      return true;
    }
  }
}
