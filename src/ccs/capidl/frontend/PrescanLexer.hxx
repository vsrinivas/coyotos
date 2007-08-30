#ifndef PRESCANLEXER_HXX
#define PRESCANLEXER_HXX

/*
 * Copyright (C) 2006, The EROS Group, LLC.
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

/* We need to include this file into the lex file so that the class
 * declaration for IdlLexer2 is available. In all other inclusions we
 * are going to need FlexLexer.h, but if we include that in the lexer
 * we will get a redundantly defined lexer class.
 *
 * It proves that FlexLexer.h is included into the lex.l output after
 * YYSTATE is defined, so use the presence or absence of YYSTATE to
 * decide whether to include FlexLexer.h.
 */
#ifndef YYSTATE

#undef yyFlexLexer
#define yyFlexLexer prescanFlexLexer
#include <FlexLexer.h>

#endif /* YYSTATE */

#include <libsherpa/Path.hxx>
#include <libsherpa/LexLoc.hxx>
#include "IdlLexer.hxx"
#include "TopSym.hxx"

class PrescanLexer : public IdlLexer {
public:
  std::string pkgName;
  GCPtr< CVector< GCPtr<TopSym> > > map;

  enum {
    ls_package,			// looking for "package" keyword
    ls_pkgident,		// looking for package name.
    ls_topkwd,			// looking for top-level keyword
    ls_topdef,			// looking for top-level definition
    ls_munch			// munching a scope
  } lexState;

  bool isCmdLine;
  //  int commentCaller;

  PrescanLexer(std::istream&, GCPtr<Path> inputFileName, 
	       GCPtr< CVector< GCPtr<TopSym> > > uocMap,
	       bool isCmdLine);

  // This lexer is called for side effects. It only returns success or
  // failure.
  bool lex();
};

#endif /* PRESCANLEXER_HXX */
