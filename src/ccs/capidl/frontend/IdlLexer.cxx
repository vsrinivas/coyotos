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
#include <libsherpa/LexLoc.hxx>
#include <libsherpa/utf8.hxx>

#include "../BUILD/idl.hxx"

#include "IdlLexer.hxx"

using namespace sherpa;
using namespace std;

struct KeyWord {
  const char *nm;
  int  tokValue;
} keywords[] = {
  { "abstract",		tk_ABSTRACT},
  { "an",		tk_AN},
  { "any",		tk_ANY},
  { "array",		tk_ARRAY},
  { "as",		tk_AS},
  { "attribute",	tk_ATTRIBUTE},
  { "begin",		tk_BEGIN},
  { "behalf",		tk_BEHALF},
  { "bind",		tk_BIND},
  { "bitset",		tk_BITSET},
  { "boolean",		tk_BOOLEAN},
  { "buffer",		tk_BUFFER},
  { "byte",		tk_BYTE},
  { "case",		tk_CASE},
  { "catch",		tk_CATCH},
  { "char",		tk_CHAR},
  { "class",		tk_CLASS},
  { "client",		tk_CLIENT},
  { "const",		tk_CONST},
  { "context",		tk_CONTEXT},
  { "default",		tk_DEFAULT},
  { "double",		tk_DOUBLE},
  { "else",		tk_ELSE},
  { "ensure",		tk_ENSURE},
  { "enum",		tk_ENUM},
  { "escape",		tk_ESCAPE},
  { "eventual",		tk_EVENTUAL},
  { "eventually",	tk_EVENTUALLY},
  { "exception",	tk_EXCEPTION},
  { "export",		tk_EXPORT},
  { "extends",		tk_EXTENDS},
  { "facet",		tk_FACET},
  { "false",		tk_FALSE},
  { "finally",		tk_FINALLY},
  { "fixed",		tk_FIXED},
  { "float",		tk_FLOAT},
  { "forall",		tk_FORALL},
  { "for",		tk_FOR},
  { "function",		tk_FUNCTION},
  { "implements",	tk_IMPLEMENTS},
  { "in",		tk_IN},
  { "inherits",		tk_INHERITS},
  { "inout",		tk_INOUT},
  { "integer",		tk_INTEGER},
  { "interface",	tk_INTERFACE},
  { "is",		tk_IS},
  { "lambda",		tk_LAMBDA},
  { "let",		tk_LET},
  { "long",		tk_LONG},
  { "loop",		tk_LOOP},
  { "match",		tk_MATCH},
  { "max",		tk_MAX},
  { "meta",		tk_META},
  { "method",		tk_METHOD},
  { "methods",		tk_METHODS},
  { "min",		tk_MIN},
  { "module",		tk_MODULE},
  { "namespace",	tk_NAMESPACE},
  { "native",		tk_NATIVE},
  { "nostub",		tk_NOSTUB},
  { "object",		tk_OBJECT},
  { "on",		tk_ON},
  { "oneway",		tk_ONEWAY},
  { "out",		tk_OUT},
  { "package",		tk_PACKAGE},
  { "private",		tk_PRIVATE},
  { "protected",	tk_PROTECTED},
  { "public",		tk_PUBLIC},
  { "raises",		tk_RAISES},
  { "readonly",		tk_READONLY},
  { "reliance",		tk_RELIANCE},
  { "reliant",		tk_RELIANT},
  { "relies",		tk_RELIES},
  { "rely",		tk_RELY},
  { "repr",		tk_REPR},
  { "reveal",		tk_REVEAL},
  { "sake",		tk_SAKE},
  { "sequence",		tk_SEQUENCE},
  { "short",		tk_SHORT},
  { "signed",		tk_SIGNED},
  { "static",		tk_STATIC},
  { "string",		tk_STRING},
  { "struct",		tk_STRUCT},
  { "supports",		tk_SUPPORTS},
  { "suspects",		tk_SUSPECTS},
  { "suspect",		tk_SUSPECT},
  { "switch",		tk_SWITCH},
  { "synchronize",	tk_SYNCHRONIZED},
  { "this",		tk_THIS},
  { "throws",		tk_THROWS},
  { "to",		tk_TO},
  { "transient",	tk_TRANSIENT},
  { "true",		tk_TRUE},
  { "truncatable",	tk_TRUNCATABLE},
  { "try",		tk_TRY},
  { "typedef",		tk_TYPEDEF},
  { "union",		tk_UNION},
  { "unsigned",		tk_UNSIGNED},
  { "uses",		tk_USES},
  { "using",		tk_USING},
  { "utf16",		tk_UTF16},
  { "utf8",		tk_UTF8},
  { "valuetype",	tk_VALUETYPE},
  { "virtual",		tk_VIRTUAL},
  { "void",		tk_VOID},
  { "volatile",		tk_VOLATILE},
  { "wchar",		tk_WCHAR},
  { "when",		tk_WHEN},
  { "while",		tk_WHILE},
  { "wstring",		tk_WSTRING},

  { 0,                  0 }	// sentinal
};

static int
kwstrcmp(const void *vKey, const void *vCandidate)
{
  const char *key = ((const KeyWord *) vKey)->nm;
  const char *candidate = ((const KeyWord *) vCandidate)->nm;

  if (candidate == 0)
    return -1;

  return strcmp(key, candidate);
}

int
IdlLexer::kwCheck(const char *s)
{
#if 0
  if (ifIdentMode) {
    if (!valid_ifident_start(*s))
      return tk_Reserved;

    for (++s; *s; s++)
      if (!valid_ifident_continue(*s))
	return tk_Reserved;

    return tk_Ident;
  }
#endif

  KeyWord key = { s, 0 };
  KeyWord *entry = 
    (KeyWord *)bsearch(&key, keywords,
		       sizeof(keywords)/sizeof(keywords[0]), 
		       sizeof(keywords[0]), kwstrcmp);

  // If it is in the token table, return the indicated token type:
  if (entry)
    return entry->tokValue;

  return tk_Identifier;
}

IdlLexer::IdlLexer(std::istream& _in, GCPtr<Path> inputPath,
		   bool _isActiveUOC)
  : UCSLexer(_in, inputPath)
{
  isActiveUOC = _isActiveUOC;
  ifIdentMode = false;

  doDocComments = true;

  currentDocComment = NULL;
}

// Slurps a comment, and processes it as a Doc Comment, if necessary.
// XXX needs updating
// For C-style comments, the operation is simple:  read in /*[*]...*/.  For
// C++-style comments, we have to read ahead to the next line, processing
// whitespace until we see '//' or '///'.  This means that we have to have
// three characters of ungetc, in order to catch:
//
//    /// doc comment here
//    /// more doc comment
//    // normal comment
//
// correctly.  This function relies on the fact that whitespace is ignored
// by the lexer.
//
bool
IdlLexer::grabCComment()
{
  if (!slurpCComment())
    return false;

  if (!doDocComments)
    return true;

  LexLoc docCommentStart = here; // if this is one.

  if (thisToken.substr(0, 3) != "/**")
    return true;

  DocComment *doc = new DocComment();
  try {
    doc->ProcessComment(docCommentStart, thisToken, DocComment::CComment);
    currentDocComment = doc;

  } catch (DocComment::ParseFailureException e) {
    ReportParseWarning(e.loc, e.error);
    // Avoid accepting a previous doc comment that this one would
    // have replaced:
    currentDocComment = NULL;
  }

  return true;
}

bool
IdlLexer::grabCxxComment()
{
  ucs4_t c = getChar();

  LexLoc docCommentStart = here; // if it is one.

  if (!doDocComments || (c != '/')) {
    ungetChar(c);
    return slurpCxxComment();
  }

  if (!slurpCxxDocComment())
    return false;

  DocComment *doc = new DocComment();
  try {
    doc->ProcessComment(docCommentStart, thisToken, DocComment::CxxComment);
    currentDocComment = doc;

    return (true);

  } catch (DocComment::ParseFailureException e) {
    ReportParseWarning(e.loc, e.error);
    // Avoid accepting a previous doc comment that this one would
    // have replaced:
    currentDocComment = NULL;
  }

  return true;
}

DocComment *
IdlLexer::grabDocComment()
{
  DocComment *ret = currentDocComment;
  currentDocComment = NULL;
  return ret;
}

#define BAD_TOKEN EOF
int
IdlLexer::idllex(IdlParseType *lvalp)
{
  ucs4_t c;

 startOver:
  thisToken.erase();
  nDigits = 0;			// until otherwise proven
  radix = 10;			// until otherwise proven

  c = getChar();

  switch (c) {
  case ':':			// Possible ::, else :
    c = getChar();
    if (c != ':') {
      ungetChar(c);
      c = ':';
      goto single_char_token;
    }

    lvalp->tok = LToken(here, thisToken);
    here.updateWith(thisToken);
    return tk_OPSCOPE;

  case '/':			// Possible comment start, else divide
    c = getChar();
    if (c == '/') {
      if (grabCxxComment() == false)
	return EOF;
      goto startOver;

    } 
    if (c == '*') {
      if (grabCComment() == false)
	return EOF;
      goto startOver;

    }
    ungetChar(c);
    c = '/';
    goto single_char_token;
 
  case ' ':			// White space
  case '\t':
  case '\r':
  case '\n':
    here.updateWith(thisToken);
    goto startOver;

  case '<':		// single character tokens
  case '>':
  case '=':
  case '-':
  case ',':
  case ';':
  case '.':
  case '(':
  case ')':
  case '[':
  case ']':
  case '{':
  case '}':
  case '*':
  case '+':

  single_char_token:
    lvalp->tok = LToken(here, thisToken);
    here.updateWith(thisToken);
    return c;

  case '"':			// String literal
    {
      do {
	c = getChar();

	if (c == '\\') 
	  (void) getChar();	// just ignore it -- will validate later

      }	while (c != '"');
      
      if (!validateString()) 
	return BAD_TOKEN;

      LexLoc tokStart = here;
      here.updateWith(thisToken);
      lvalp->tok = LToken(here, thisToken.substr(1, thisToken.size()-2));
      return tk_StringLiteral;
    }


    // We do not (currently) accept character constants.

#if 0
    // Since this is an infix language, we need to deal with things
    // like

    //    1 - -1
    //    1 - 1
    //    1-1
    //
    // This is best handled in the parser, so I have made hyphen a
    // single character token and numbers do not have a leading sign.
  case '-':
    c = getChar();
    if (digitValue(c) < 0) {
      ungetChar(c);
      goto identifier;
    }
#endif

  case '0':			// Numbers
    {
      radix = 8;		// initial 0 -- until otherwise proven

      c = getChar();
      if (c == 'x' || c == 'X') {
	radix = 16;

	// 0x must be followed by digits. Check that here.

	c = getChar();
	ungetChar(c);

	if (digitValue(c) < 0)
	  return BAD_TOKEN;
      }
      else
	ungetChar(c);

      // FALL THROUGH
    }

  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    {
      do {
	c = getChar();
      } while (digitValue(c) >= 0);
      ungetChar(c);

      /* If we were accepting floating literals, we would check for
	 decimal point here. */

      lvalp->tok = LToken(here, thisToken);
      here.updateWith(thisToken);
      return tk_IntegerLiteral;
    }

  case EOF:
    return EOF;

  default:
    if (validIdentStart(c))
      goto identifier;

    // FIX: Malformed token
    return BAD_TOKEN;
  }

 identifier:
  do {
    c = getChar();
  } while (validIdentContinue(c));
  ungetChar(c);

  lvalp->tok = LToken(here, thisToken);
  here.updateWith(thisToken);
  return kwCheck(thisToken.c_str());
}
