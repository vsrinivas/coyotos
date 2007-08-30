/*
 * Copyright (C) 2002, The EROS Group, LLC.
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

/* Scanner for new and improved capidl program */

/* CPP_LINE lexemes are retained for backwards compatibility
   support. */

#include <sys/types.h>
/* GNU multiple precision library: */
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>

#include <string>

#include "SymTab.hxx"
#include "PrescanLexer.hxx"

using namespace std;
using namespace sherpa;

#include "IdlParseType.hxx"
#include "../BUILD/idl.hxx"

bool
PrescanLexer::lex()
{
  // Just run the IdlLexer and watch the token stream. What a hack!
  // (Suggested by Mr. Adams).

  int curlyDepth = 0;

  IdlParseType curToken;

  for (;;) {
    LexLoc oldHere = here;
    int tok = idllex(&curToken);
    
    switch (lexState) {
    case ls_package:
      {
	if (tok == tk_PACKAGE) {
	  lexState = ls_pkgident;
	  continue;
	}

	ReportParseError(curToken.tok.loc, "Expected token \"package\"");
	return false;
      }
    case ls_pkgident:
      {
	if (tok == tk_Identifier || tok == '.') {
	  pkgName = pkgName + thisToken;
	  continue;
	}

	if (tok == ';') {
	  lexState = ls_topkwd;
	  continue;
	}
	  
	ReportParseError(curToken.tok.loc, "Expected token \"package\"");
	return false;
      }

    case ls_topkwd:
      {
	switch(tok) {
	case tk_ABSTRACT:
	  // modifier on INTERFACE
	  continue;

	case tk_INTERFACE:
	case tk_STRUCT:
	case tk_UNION:
	case tk_EXCEPTION:
	case tk_ENUM:
	case tk_NAMESPACE:
	  lexState = ls_topdef;
	  continue;

	case EOF:
	  return true;

	default:
	  {
	    ReportParseError(curToken.tok.loc, "Expected a top-level keyword");
	    return false;
	  }
	}
      }

    case ls_topdef:
      {
	if (tok == tk_Identifier) {
	  std::string topName = pkgName + "." + thisToken;
	  GCPtr<TopSym> tsm = new TopSym(topName, curToken.tok.loc.path, isCmdLine);
	  map->append(tsm);

	  lexState = ls_munch;
	  continue;
	}

	ReportParseError(curToken.tok.loc, "Expected a top-level identifier");
	return false;
      }

    case ls_munch:
      {
	switch(tok) {
	case '{':
	case '(':
	case '[':
	  curlyDepth++;
	  continue;

	case '}':
	case ')':
	case ']':
	  {
	    curlyDepth--;
	    continue;
	  }
	case ';':
	  {
	    if (curlyDepth == 0)
	      lexState = ls_topkwd;

	    continue;
	  }
	default:
	  continue;
	}
      }
    }
  }
}

PrescanLexer::PrescanLexer(std::istream& in, GCPtr<Path> inFile, 
			   GCPtr< CVector<GCPtr<TopSym> > >uocMap,
			   bool isCmdLine)
  : IdlLexer(in, inFile, false /* don't care */)
{
  map = uocMap;
  lexState = ls_package;

  doDocComments = false;

  this->isCmdLine = isCmdLine;
}

TopSym::TopSym(const std::string& s, GCPtr<Path> f, bool isCmdLine)
{
  symName = s;
  fileName = f;
  this->isCmdLine = isCmdLine;
  isUOC = true;
}
