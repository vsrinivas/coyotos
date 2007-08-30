%{
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

/*
 * This file contains an implementation of the CapIDL grammar.
 *
 * CapIDL was originally defined by Mark Miller, with much feedback
 * from the eros-arch list, especially Jonathan Shapiro. CapIDL is in
 * turn distantly derived from the Corba IDL, by the OMG.
 *
 * Further information on CapIDL can be found at http://www.capidl.org
 *
 * This grammar differs from the base CapIDL grammar as follows:
 *
 *   1. It is a subset. Various pieces of the CapIDL grammar are
 *      omitted until I can implement them and test them
 *      satisfactorily.
 *
 *   2. It uses fixed-precision integer arithmetic for constants. I
 *      believe that this deviation from CORBA was unjustified. Mark
 *      and I have yet to sort this out, and it is possible that this
 *      tool will switch to true integer arithmetic at some time in
 *      the future.
 *
 *   3. Tokens and productions are all properly typed.
 *
 *   4. It actually has an implementation. I have endeavoured to
 *      strongly separate the code generator from the parser, in order
 *      that the code generator can be easily modified by others. In
 *      particular, I have already reached the point where I have
 *      decided that there will be distinct code generators for C and
 *      for C++, if only to allow me to validate the merits of the
 *      proposed exception handling model.
 *
 *   5. tk_RESERVED is a token, not a production. Catching that error in
 *      the grammar only makes things uglier.
 *
 *
 * This grammar probably requires Bison, as I think it's really really
 * stupid to have a parser that must be reinitialized in order to
 * process more than one file.
 *
 * Note that capidl (the tool) tries hard NOT to be a real
 * compiler. The purpose of this implementation is to translate
 * straightforward input into straightforward output. The one place
 * where this should not be true is in the client-side stub
 * generator. The current client stub generator is quite inefficient,
 * but I did not want to try making one that was more efficient when
 * the capability invocation specification for EROS itself is
 * presently up in the air.
 */

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>


#include <string>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;


#include "SymTab.hxx"
#include "IdlParseType.hxx"
#include "IdlLexer.hxx"

extern bool showparse;	/* global debugging flag */

#define SHOWPARSE(s) if (showparse) fprintf(stderr, s)
#define SHOWPARSE1(s,x) if (showparse) fprintf(stderr, s,x)

int num_errors = 0;  /* hold the number of syntax errors encountered. */

#define YYSTYPE IdlParseType

#define YYLEX_PARAM (IdlLexer *)lexer

#undef yyerror
#define yyerror(lexer, s) lexer->ReportParseError(s)

void import_symbol(const std::string& ident);
inline int idllex(YYSTYPE *lvalp, IdlLexer *lexer)
  {
    return lexer->idllex(lvalp);
  }
extern void output_symdump(GCPtr<Symbol> );
%}

%pure_parser
%parse-param {IdlLexer *lexer}

/* Categorical terminals */
%token <tok>        tk_Identifier
%token <tok>        tk_IntegerLiteral
%token <tok>        tk_CharLiteral
%token <tok>        tk_FloatingPtLiteral
%token <tok>        tk_StringLiteral
 


/* Corba Keywords */
%token <tok>  tk_BOOLEAN tk_CASE tk_CHAR tk_CONST tk_DEFAULT 
%token <tok>  tk_DOUBLE tk_ENUM tk_EXCEPTION tk_FLOAT tk_BITSET
%token <tok>  tk_TRUE tk_FALSE
%token <tok>  tk_MIN tk_MAX
%token <tok>  tk_INTERFACE tk_LONG tk_MODULE tk_OBJECT tk_BYTE tk_ONEWAY tk_OUT
%token <tok>  tk_RAISES tk_SHORT tk_STRING tk_STRUCT tk_SWITCH tk_TYPEDEF
%token <tok>  tk_UNSIGNED tk_UNION tk_VOID tk_WCHAR tk_WSTRING

/* Other Keywords */
%token <tok>  tk_INTEGER tk_REPR 
%token <tok>  '+' '-' '*' '/'

%token <tok>  tk_INHERITS

/* Reserved Corba Keywords */
%token <tok>  tk_ANY tk_ATTRIBUTE tk_CONTEXT tk_FIXED tk_IN tk_INOUT tk_NATIVE 
%token <tok>  tk_READONLY tk_TRUNCATABLE tk_VALUETYPE
%token <tok>  tk_SEQUENCE tk_BUFFER tk_ARRAY

%token <tok>  tk_CLIENT
%token <tok>  tk_NOSTUB
%type  <flags> opr_qual

/* Other Reserved Keywords. Note that BEGIN conflicts with flex usage
 * for input state transitions, so we cannot use that. */
%token <tok>  tk_ABSTRACT tk_AN tk_AS tk_BEGIN tk_BEHALF tk_BIND 
%token <tok>  tk_CATCH tk_CLASS
%token <tok>  tk_DECLARE tk_DEF tk_DEFINE tk_DEFMACRO tk_DELEGATE tk_DEPRECATED
%token <tok>  tk_DISPATCH tk_DO tk_ELSE tk_END tk_ENSURE tk_EVENTUAL tk_ESCAPE
%token <tok>  tk_EVENTUALLY tk_EXPORT tk_EXTENDS  tk_FACET tk_FINALLY tk_FOR
%token <tok>  tk_FORALL tk_FUNCTION tk_IMPLEMENTS tk_IS
%token <tok>  tk_LAMBDA tk_LET tk_LOOP tk_MATCH tk_META tk_METHOD tk_METHODS 
%token <tok>  tk_NAMESPACE tk_ON 
%token <tok>  tk_PACKAGE tk_PRIVATE tk_PROTECTED tk_PUBLIC 
%token <tok>  tk_RELIANCE tk_RELIANT tk_RELIES tk_RELY tk_REVEAL
%token <tok>  tk_SAKE tk_SIGNED tk_STATIC
%token <tok>  tk_SUPPORTS tk_SUSPECT tk_SUSPECTS tk_SYNCHRONIZED
%token <tok>  tk_THIS tk_THROWS tk_TO tk_TRANSIENT tk_TRY 
%token <tok>  tk_USES tk_USING tk_UTF8 tk_UTF16 
%token <tok>  tk_VIRTUAL tk_VOLATILE tk_WHEN tk_WHILE

/* operators */
%token <tok>  tk_OPSCOPE /* :: */

%type  <NONE> start
%type  <NONE> unit_of_compilation
%type  <NONE> top_definitions
%type  <NONE> top_definition
%type  <NONE> package_dcl
%type  <sym>  if_extends
%type  <NONE> if_definitions
%type  <NONE> if_definition
%type  <tok>  name_def
%type  <tok>  namespace_dcl
%type  <NONE> namespace_members
%type  <tok>  struct_dcl
%type  <sym>  except_dcl
%type  <sym>  except_name_def
%type  <tok>  union_dcl
%type  <tok>  enum_dcl
%type  <tok>  bitset_dcl
%type  <NONE> typedef_dcl
%type  <tok>  const_dcl
%type  <NONE> element_dcl
%type  <NONE> interface_dcl
%type  <tok>  repr_dcl
%type  <sym>  type
%type  <sym>  param_type
%type  <sym>  const_expr
%type  <sym>  const_sum_expr
%type  <sym>  const_mul_expr
%type  <sym>  const_term
%type  <tok>  ident
%type  <tok>  scoped_name
%type  <tok>  dotted_name
%type  <sym>  simple_type
%type  <sym>  seq_type
%type  <sym>  buf_type
%type  <sym>  array_type
%type  <sym>  string_type
%type  <sym>  integer_type
%type  <sym>  signed_integer_type
%type  <sym>  unsigned_integer_type
%type  <sym>  float_type
%type  <sym>  char_type
%type  <sym>  switch_type
%type  <NONE> member
%type  <NONE> member_list
%type  <NONE> case
%type  <NONE> cases
%type  <NONE> case_label
%type  <NONE> case_labels
%type  <NONE> enum_defs
%type  <sym>  literal
%type  <NONE> opr_dcl
%type  <sym>  ret_type
%type  <NONE> params
%type  <NONE> param_list
%type  <sym>  param
%type  <NONE> param_2s
%type  <NONE> param_2_list
%type  <NONE> raises
%type  <NONE> advice
%type  <tok>  reserved

/* Grammar follows */
%%

start: unit_of_compilation
   { SHOWPARSE("start -> unit_of_compilation\n"); }
 ;

unit_of_compilation: package_dcl /* uses_dcls */ top_definitions
  {

    SHOWPARSE("unit_of_compilation -> "
	      "package_dcl uses_dcls top_definitions\n");
    /* We appear to have imported this package successfully. Mark the
       import completed: */

    assert(Symbol::CurScope->cls == sc_package);

    /* Pop the package scope... */
    Symbol::PopScope() ;
  };

package_dcl: tk_PACKAGE dotted_name ';'
  {
    GCPtr<Symbol> sym = 0;

    SHOWPARSE("tk_PACKAGE dotted_name\n");

    sym = Symbol::UniversalScope->LookupChild($2, 0);

    if (!sym)
      sym = Symbol::CreatePackage($2, Symbol::UniversalScope);

    sym->v.lty = lt_bool;	/* true if import is completed. */
    sym->v.bn = 0;

    Symbol::PushScope(sym);
  } ;

//uses_dcls:
//    /* empty */ { SHOWPARSE("uses_dcls -> <empty>\n"); }
//  | uses_dcls uses_dcl
//    { SHOWPARSE("uses_dcls -> uses_dcls uses_dcl\n"); }
//  ;
//
//uses_dcl: tk_USES dotted_name ';'
//  {
//    SHOWPARSE("uses_dcl -> tk_USES dotted_name\n");
//
//    /* First, see if the desired package is already imported: */
//    import_symbol($2.ltok);
//
//    GCPtr<Symbol> pkgSym = Symbol::UniversalScope->LookupChild(,$2.ltok);
//
//    GCPtr<Symbol> sym = Symbol::Create($2.ltok, sc_usepkg, 0);
//    sym->value = pkgSym;
//  }

top_definitions:
    /* empty */ { SHOWPARSE("top_definitions -> <empty>\n"); }
 | top_definitions top_definition
   { SHOWPARSE("top_definitions -> top_definitions top_definition\n");
   }
 ;

// All top-level definitions must introduce a namespace!
//
// Feb 14 2006: Actually, this isn't correct. The real issue is that
// supporting these in the prescan lexer requires a full parse, which
// in turn requires that we revise the way symbol resolution is
// handled in a fairly major way.
top_definition:
    interface_dcl ';'
    { SHOWPARSE("top_definition -> interface_dcl ';' \n"); }
  | struct_dcl ';'
    { SHOWPARSE("top_definition -> struct_dcl ';' \n"); }
  | except_dcl ';'
    { 
      SHOWPARSE("top_definition -> except_dcl ';' \n"); 
      fprintf(stderr, 
	      "%s: %s -- top level exception definitions not permitted\n",
	      $1->loc.c_str(),
	      $1->name.c_str());
      num_errors++;
    }
  | union_dcl ';'
    { SHOWPARSE("top_definition -> union_dcl ';' \n"); }
  | enum_dcl ';'
    { SHOWPARSE("top_definition -> enum_dcl ';' \n"); }
  | bitset_dcl ';'
    { SHOWPARSE("top_definition -> bitset_dcl ';' \n"); }
  | namespace_dcl ';'
    { SHOWPARSE("top_definition -> namespace_dcl ';' \n"); }
  ;

other_definition:
    typedef_dcl ';'
    { SHOWPARSE("other_definition -> typedef_dcl ';' \n"); }
  | const_dcl ';'
    { SHOWPARSE("other_definition -> const_dcl ';' \n"); }
  | repr_dcl ';'
    { SHOWPARSE("other_definition -> repr_dcl ';' \n"); }
  ;

/********************** Names ***************************/



/**
 * A defining-occurrence of an identifier.  The identifier is defined
 * as a name within the scope in which it textually appears.
 */
name_def:
        ident
    { SHOWPARSE("name_def -> ident\n"); }
 ;

/**
 * A use-occurrence of a name.  The name may be unqualified, fully
 * qualified, or partially qualified.  Corba scoping rules are used to
 * associate a use-occurrence with a defining-occurrence.
 */
scoped_name:
        dotted_name {
	  SHOWPARSE("scoped_name -> dotted_name\n");

	  $$ = $1;
	}
//  |     tk_OPSCOPE dotted_name {
//	  SHOWPARSE("scoped_name -> dotted_name\n");
//
//
//	  $$ = $2;
//	}
  ;

dotted_name:
        ident {                         /* unqualified */
	  SHOWPARSE("dotted_name -> ident\n");

	  $$ = $1;
	}
 |      dotted_name '.' ident {     /* qualified */
	  SHOWPARSE("dotted_name -> dotted_name '.' ident\n");

	  $$ = LToken($1.loc, $1.str + "." + $3.str);
        }
 ;

/**
 * These extra productions exist so that a better diagnostic can be
 * given when a reserved keyword is used where a normal identifier is
 * expected.  The reserved: production should eventually list all the
 * reserved keywords.
 */
ident:
        tk_Identifier {
          SHOWPARSE("ident -> tk_Identifier\n");
          $$ = $1; 
        }
 |      reserved   {
	  SHOWPARSE("ident -> reserved\n");
	  fprintf(stderr, "%s: %s is a reserved word\n",
		  $1.loc.c_str(),
		  $1.str.c_str());
	  num_errors++;
	  YYERROR;
	 }
 ;

/* I'm lazy -- not bothering with all the reserved SHOWPARSE calls */
reserved:
        tk_ANY | tk_ATTRIBUTE | tk_CONTEXT | tk_FIXED | tk_IN | tk_INOUT | tk_NATIVE 
 |      tk_READONLY | tk_TRUNCATABLE | tk_VALUETYPE 
 ;



/********************** Types ***************************/



/**
 * Can be a capability type, a pure data type, or a mixed type.  Most
 * of the contexts where a mixed type is syntactically accepted
 * actually require a pure data type, but this is enforced after
 * parsing.
 */
type:
	param_type
   { SHOWPARSE("type -> param_type\n"); $$ = $1;}
 |      seq_type
   { SHOWPARSE("type -> seq_type\n"); $$ = $1; }
 |      buf_type
   { SHOWPARSE("type -> buf_type\n"); $$ = $1; }
 ;

/* Sequence types cannot be paramater types, because some output 
 * languages require a struct declaration for them, and this in turn
 * requires that a type name exist for purposes of determining
 * compatibility of argument passing.
 */

param_type:
        simple_type                     /* atomic pure data value */
   { SHOWPARSE("param_type -> simple_type\n"); $$ = $1; }
 |      string_type                     /* sequences of characters */
   { SHOWPARSE("param_type -> string_type\n"); $$ = $1; }
 |      array_type
   { SHOWPARSE("param_type -> array_type\n"); $$ = $1; }
 |      tk_OBJECT                          /* generic capability */ {
          SHOWPARSE("param_type -> tk_OBJECT\n");
	  $$ = 0;
          /* FIX: Need generic interface type */
	}
 |      scoped_name {                   /* must name a defined type */
          GCPtr<Symbol> symRef = 0;

          SHOWPARSE("param_type -> scoped_name\n");

	  symRef = Symbol::CreateRef($1, lexer->isActiveUOC);

	  $$ = symRef;
	}
 ;

simple_type:
        integer_type {                  /* subranges of tk_INTEGER */
          SHOWPARSE("simple_type -> integer_type\n");
          $$ = $1;
        }
 |      float_type {              /* various IEEE precisions */
          SHOWPARSE("simple_type -> float_type\n");
          $$ = $1;
        }
 |      char_type {                     /* subranges of Unicode tk_WCHAR */
          SHOWPARSE("simple_type -> char_type\n");
          $$ = $1;
        }
 |      tk_BOOLEAN {                       /* TRUE or FALSE */
          SHOWPARSE("simple_type -> tk_BOOLEAN\n");
	  $$ = Symbol::KeywordScope->LookupChild("#bool", 0);
	}
 ;

/**
 * The only values of these types are integers.  The individual types
 * differ regarding what subrange of integer they accept.  Not all
 * syntactically expressible variations will supported in the
 * forseable future.  We expect to initially support only tk_INTEGER<N>
 * and tk_UNSIGNED<N> for N == 16, 32, 64, as well as tk_UNSIGNED<8>.
 */
/* SHAP: (1) ranges are a content qualifier, not a type
         (2) ranges temporarily ommitted -- grammar needs revision to
             get them right.
*/
integer_type :
        signed_integer_type {
          SHOWPARSE("integer_type -> signed_integer_type\n");
          $$ = $1;
        }
 |      unsigned_integer_type {
          SHOWPARSE("integer_type -> unsigned_integer_type\n");
          $$ = $1;
        }

signed_integer_type:
        tk_INTEGER                         /* == tk_INTEGER<8> */ {
          SHOWPARSE("signed_integer_type -> tk_INTEGER\n");
          $$ = Symbol::KeywordScope->LookupChild("#int", 0);
        }
 |      tk_INTEGER '<' const_expr '>'      /* [-2**(N-1),2**(N-1)-1] */ {
          SHOWPARSE("signed_integer_type -> tk_INTEGER '<' const_expr '>'\n");

	  if ($3->v.bn == 8)
	    $$ = Symbol::KeywordScope->LookupChild("#int8", 0);
	  else if ($3->v.bn == 16)
	    $$ = Symbol::KeywordScope->LookupChild("#int16", 0);
	  else if ($3->v.bn == 32)
	    $$ = Symbol::KeywordScope->LookupChild("#int32", 0);
	  else if ($3->v.bn == 64)
	    $$ = Symbol::KeywordScope->LookupChild("#int64", 0);
	  else {
	    fprintf(stderr, "%s: syntax error -- bad integer size\n",
		    $3->loc.c_str());
	    num_errors++;
	    YYERROR;
	    break;
	  }
        }
 |      tk_BYTE                            /* == tk_INTEGER<8> */ {
          SHOWPARSE("signed_integer_type -> tk_BYTE\n");
          $$ = Symbol::KeywordScope->LookupChild("#int8", 0);
        }
 |      tk_SHORT                           /* == tk_INTEGER<16> */ {
          SHOWPARSE("signed_integer_type -> tk_SHORT\n");
          $$ = Symbol::KeywordScope->LookupChild("#int16", 0);
        }
 |      tk_LONG                            /* == tk_INTEGER<32> */ {
          SHOWPARSE("signed_integer_type -> tk_LONG\n");
          $$ = Symbol::KeywordScope->LookupChild("#int32", 0);
        }
 |      tk_LONG tk_LONG                       /* == tk_INTEGER<64> */ {
          SHOWPARSE("signed_integer_type -> tk_LONG tk_LONG\n");
          $$ = Symbol::KeywordScope->LookupChild("#int64", 0);
        }

 ;

unsigned_integer_type:
        tk_UNSIGNED '<' const_expr '>'     /* [0,2**N-1] */ {
          SHOWPARSE("unsigned_integer_type -> tk_UNSIGNED '<' const_expr '>'\n");
	  if ($3->v.bn == 8)
	    $$ = Symbol::KeywordScope->LookupChild("#uint8", 0);
	  else if ($3->v.bn == 16)
	    $$ = Symbol::KeywordScope->LookupChild("#uint16", 0);
	  else if ($3->v.bn == 32)
	    $$ = Symbol::KeywordScope->LookupChild("#uint32", 0);
	  else if ($3->v.bn == 64)
	    $$ = Symbol::KeywordScope->LookupChild("#uint64", 0);
	  else {
	    fprintf(stderr, "%s: syntax error -- bad integer size\n",
		    $3->loc.c_str());
	    num_errors++;
	    YYERROR;
	    break;
	  }
        }
 |      tk_UNSIGNED tk_BYTE                   /* == tk_UNSIGNED<8> */ {
          SHOWPARSE("unsigned_integer_type -> tk_UNSIGNED tk_BYTE\n");
          $$ = Symbol::KeywordScope->LookupChild("#uint8", 0);
        }
 |      tk_UNSIGNED tk_SHORT                  /* == tk_UNSIGNED<16> */ {
          SHOWPARSE("unsigned_integer_type -> tk_UNSIGNED tk_SHORT\n");
          $$ = Symbol::KeywordScope->LookupChild("#uint16", 0);
        }
 |      tk_UNSIGNED tk_LONG                   /* == tk_UNSIGNED<32> */ {
          SHOWPARSE("unsigned_integer_type -> tk_UNSIGNED tk_LONG\n");
          $$ = Symbol::KeywordScope->LookupChild("#uint32", 0);
        }
 |      tk_UNSIGNED tk_LONG tk_LONG              /* == tk_UNSIGNED<64> */ {
          SHOWPARSE("unsigned_integer_type -> tk_UNSIGNED tk_LONG tk_LONG\n");
          $$ = Symbol::KeywordScope->LookupChild("#uint64", 0);
        }
 ;

/**
 * The only values of these types are real numbers, positive and
 * negative infinity, and the NaNs defined by IEEE.  As each IEEE
 * precision is a unique beast, the sizes may only be those defined as
 * standard IEEE precisions.  We expect to initially support only
 * tk_FLOAT<32> and tk_FLOAT<64>.
 */
float_type:
        tk_FLOAT '<' const_expr '>'      /* IEEE std floating precision N */ {
          SHOWPARSE("float_type -> tk_FLOAT '<' const_expr '>'\n");
	  if ($3->v.bn == 32)
	    $$ = Symbol::KeywordScope->LookupChild("#float32", 0);
	  else if ($3->v.bn == 64)
	    $$ = Symbol::KeywordScope->LookupChild("#float64", 0);
	  else if ($3->v.bn == 128)
	    $$ = Symbol::KeywordScope->LookupChild("#float128", 0);
	  else {
	    fprintf(stderr, "%s: syntax error -- unknown float size\n",
		    $3->loc.c_str());
	    num_errors++;
	    YYERROR;
	    break;
	  }
	}
 |      tk_FLOAT                         /* == tk_FLOAT<32> */ {
          SHOWPARSE("float_type -> tk_FLOAT\n");
          $$ = Symbol::KeywordScope->LookupChild("#float32", 0);
	}
 |      tk_DOUBLE                        /* == tk_FLOAT<64> */ {
          SHOWPARSE("float_type -> tk_DOUBLE\n");
          $$ = Symbol::KeywordScope->LookupChild("#float64", 0);
	}
 |      tk_LONG tk_DOUBLE                   /* == tk_FLOAT<128> */ {
          SHOWPARSE("float_type -> tk_LONG tk_DOUBLE\n");
          $$ = Symbol::KeywordScope->LookupChild("#float128", 0);
	}
 ;

/**
 * The only values of these types are 32 bit Unicode characters.
 * These types differ regarding the subrange of Unicode character
 * codes they will accept.  We expect to initially support only
 * tk_WCHAR<7> (ascii), tk_WCHAR<8> (latin-1), tk_WCHAR<16> (java-unicode), and
 * tk_WCHAR<32> (full unicode). 
 */
/* SHAP: (1) Ascii is a subrange of char, not a distinct type.
         (2) ranges should work on all wchar variants.
	 (3) ranges are a content qualifier, not a type
         (4) ranges temporarily ommitted -- grammar needs revision to
             get them right.
*/
char_type:
        tk_WCHAR '<' const_expr '>'        /* tk_WCHAR<0,2**N-1> */ {
          SHOWPARSE("char_type -> tk_WCHAR '<' const_expr '>'\n");
	  if ($3->v.bn == 8)
	    $$ = Symbol::KeywordScope->LookupChild("#wchar8", 0);
	  else if ($3->v.bn == 16)
	    $$ = Symbol::KeywordScope->LookupChild("#wchar16", 0);
	  else if ($3->v.bn == 32)
	    $$ = Symbol::KeywordScope->LookupChild("#wchar32", 0);
	  else {
	    fprintf(stderr, "%s: syntax error -- bad wchar size\n",
		    $3->loc.c_str());
	    num_errors++;
	    YYERROR;
	    break;
	  }
	}
 |      tk_CHAR             /* tk_WCHAR<8> */ {
          SHOWPARSE("char_type -> tk_CHAR\n");
          $$ = Symbol::KeywordScope->LookupChild("#wchar8", 0);
        } 
 |      tk_WCHAR            /* tk_WCHAR<32> == Unicode character */ {
          SHOWPARSE("char_type -> tk_WCHAR\n");
          $$ = Symbol::KeywordScope->LookupChild("#wchar32", 0);
        } 
 ;

/**
 * A sequence is some number of repetitions of some base type.  The
 * number of repetitions may be bounded or unbounded.  Strings are
 * simply sequences of characters, but are singled out as special for
 * three reasons: 1) The subrange of character they repeat does not
 * include '\0'. 2) The marshalled representation includes an extra
 * '\0' character after the end of the string. 3) Many languages have
 * a special String data type to which this must be bound.
 */

seq_type:
        tk_SEQUENCE '<' type '>' {	// unbounded number of repetitions of type
	  GCPtr<Symbol> seqSym = Symbol::GenSym_inScope(sc_seqType, lexer->isActiveUOC, 0);
	  seqSym->type = $3;
	  fprintf(stderr, "%s: Unbounded sequence types are not (yet) supported by CapIDL\n",
		  $1.loc.c_str());
	  num_errors++;
	  YYERROR;
	  $$ = seqSym;
	}
 |      tk_SEQUENCE '<' type ',' const_expr '>' { // no more than N repetitions of type
	  GCPtr<Symbol> seqSym = Symbol::GenSym_inScope(sc_seqType, lexer->isActiveUOC, 0);
	  seqSym->type = $3;
	  seqSym->value = $5;
	  $$ = seqSym;
        }
 ;

buf_type:
        tk_BUFFER '<' type '>' {	// unbounded number of repetitions of type
	  GCPtr<Symbol> seqSym = Symbol::GenSym_inScope(sc_bufType, lexer->isActiveUOC, 0);
	  seqSym->type = $3;
	  fprintf(stderr, "%s: Buffer types are not (yet) supported by CapIDL\n",
		  $1.loc.c_str());
	  $$ = seqSym;
	}
 |      tk_BUFFER '<' type ',' const_expr '>' { // no more than N repetitions of type
	  GCPtr<Symbol> seqSym = Symbol::GenSym_inScope(sc_bufType, lexer->isActiveUOC, 0);
	  seqSym->type = $3;
	  seqSym->value = $5;
	  fprintf(stderr, "%s: Buffer types are not (yet) supported by CapIDL\n",
		  $1.loc.c_str());
	  $$ = seqSym;
        }
 ;

array_type:
        tk_ARRAY '<' type ',' const_expr '>' { // exactly N repetitions of type
	  GCPtr<Symbol> seqSym = Symbol::GenSym_inScope(sc_arrayType, lexer->isActiveUOC, 0);
	  seqSym->type = $3;
	  seqSym->value = $5;
	  $$ = seqSym;
	}
 ;

/**
   Contrary to MarkM's original design assumption (above), strings are 
   NOT simply sequence types with a reserved terminator. The reason is
   that they can usually be bound to a distinguished abstract
   source-language type. This implies that the type of tk_STRING is not
   necessarily the same as the type sequence<char> (array of
   characters).

   A curious result of this is that unbounded sequence types probably
   should not be accepted as parameter or return types, because it
   isn't at all simple to generate the appropriate wrapper structures
   on the fly to make the parameter passing work out.
*/
string_type:
        tk_STRING {
          $$ = Symbol::KeywordScope->LookupChild("#wstring8", 0);
        }
// |      tk_STRING '<' const_expr '>' {     // == tk_WCHAR<8>[N]
//	  GCPtr<Symbol> seqSym = Symbol::GenSym(sc_seqType, 0);
//	  seqSym->type = Symbol::KeywordScope->LookupChild("#wchar8");
//	  seqSym->value = $3;
//	  $$ = seqSym;
//        }
 |      tk_WSTRING {                       // == tk_WCHAR<32>[]
          $$ = Symbol::KeywordScope->LookupChild("#wstring32", 0);
        }
// |      tk_WSTRING '<' const_expr '>' {    // == tk_WCHAR<32>[N]
//	  GCPtr<Symbol> seqSym = Symbol::GenSym(sc_seqType, 0);
//	  seqSym->type = Symbol::KeywordScope->LookupChild("#wchar32");
//	  seqSym->value = $3;
//	  $$ = seqSym;
//        }
 ;


typedef_dcl:
        tk_TYPEDEF type name_def {
	  GCPtr<Symbol> sym = 0;
          SHOWPARSE("typedef_dcl -> tk_TYPEDEF type name_def\n");
	  sym = Symbol::Create($3, lexer->isActiveUOC, sc_typedef);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- typedef identifier \"%s\" reused\n",
		    $3.loc.c_str(),
		    $3.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = $2;
	  sym->docComment = lexer->grabDocComment();
	}
 ;


namespace_dcl:
        tk_NAMESPACE name_def '{'  {
	  GCPtr<Symbol> sym = Symbol::Create($2, lexer->isActiveUOC, sc_scope);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- namespace identifier \"%s\" reused\n",
		    $2.loc.c_str(),
		    $2.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->docComment = lexer->grabDocComment();

	  /* Struct name is a scope for its formals: */
	  Symbol::PushScope(sym);
	}
        namespace_members '}'  {
          SHOWPARSE("namespace_dcl -> tk_NAMESPACE name_def '{' namespace_members '}'\n");
	  Symbol::PopScope();
	  $$ = $2;
	}
 ;

namespace_members:
	/* empty */ { }
 |      namespace_members top_definition { }
 |      namespace_members other_definition { }
 ;

/********************** Structs ***************************/



/**
 * Like a C struct, a struct defines an aggregate type consisting of
 * each of the member types in order.  Whereas the members of a
 * sequence are accessed by numeric index, the members of a structure
 * are accessed by member name (ie, field name). <p>
 *
 * Like a Corba or CapIDL module, a CapIDL struct also defines a named
 * scope, such that top_definitions between the curly brackets define
 * names in that scope.
 */
struct_dcl:
        tk_STRUCT name_def '{'  {
	  GCPtr<Symbol> sym = Symbol::Create($2, lexer->isActiveUOC, sc_struct);
	  if (!sym) {
	    fprintf(stderr, 
		    "%s: syntax error -- struct identifier \"%s\" reused\n",
		    $2.loc.c_str(),
		    $2.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->docComment = lexer->grabDocComment();

	  /* Struct name is a scope for its formals: */
	  Symbol::PushScope(sym);
	}
        member_list '}'  {
          SHOWPARSE("struct_dcl -> tk_STRUCT name_def '{' member_list '}'\n");
	  Symbol::PopScope();
	  $$ = $2;
	}
 ;

member_list:
        member_list member
   { SHOWPARSE("member_list -> member_list member\n"); }
 |      member
   { SHOWPARSE("member_list -> member\n"); }
 ;

/*SHAP:        top_definition         /* defines a name inside this scope */
member:
        struct_dcl ';'
   { SHOWPARSE("member -> struct_dcl ';'\n"); }
 |      union_dcl  ';'
   { SHOWPARSE("member -> union_dcl ';'\n"); }
 |      enum_dcl ';'
   { SHOWPARSE("member -> enum_dcl ';'\n"); }
 |      bitset_dcl ';'
   { SHOWPARSE("member -> bitset_dcl ';'\n"); }
 | namespace_dcl ';'
    { SHOWPARSE("member -> namespace_dcl ';' \n"); }
 |      const_dcl ';'
   { SHOWPARSE("member -> const_dcl ';'\n"); }
 |      repr_dcl ';'
   { SHOWPARSE("member -> repr_dcl ';'\n"); }
 |      element_dcl ';'
   { SHOWPARSE("member -> element_dcl ';'\n"); }
 ;



/********************** Exceptions ***************************/


/**
 * Structs that are sent (as in tk_RAISES) to explain problems
 */
except_dcl:
        except_name_def '{' {
	  /* Struct name is a scope for its formals: */
	  Symbol::PushScope($1);
        }
        member_list '}' {
          SHOWPARSE("except_dcl -> except_name_def '{' member_list '}'\n");
	  Symbol::PopScope();
          $$ = $1;
	}
|      except_name_def /* '{' '}' */ { 
          SHOWPARSE("except_dcl -> except_name_def '{' '}'\n");
          $$ = $1;
        }
 ;


except_name_def:
        tk_EXCEPTION name_def '=' const_expr {
	  GCPtr<Symbol> sym = 0;
          SHOWPARSE("except_name_def -> tk_EXCEPTION name_def '=' const_expr\n");
	  sym = Symbol::Create($2, lexer->isActiveUOC, sc_exception);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- exception identifier \"%s\" reused\n",
		    $2.loc.c_str(),
		    $2.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = $4;
	  sym->type = Symbol::KeywordScope->LookupChild("#int", 0);
	  sym->docComment = lexer->grabDocComment();

          $$ = sym;
        }
 |      tk_EXCEPTION name_def {
	  GCPtr<Symbol> sym = 0;
          SHOWPARSE("except_name_def -> tk_EXCEPTION name_def\n");
	  sym = Symbol::Create($2, lexer->isActiveUOC, sc_exception);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- exception identifier \"%s\" reused\n",
		    $2.loc.c_str(),
		    $2.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = Symbol::MakeIntLit(LToken($2.loc, "0"));
	  sym->type = Symbol::VoidType;
	  sym->docComment = lexer->grabDocComment();

          $$ = sym;
        }
;
/********************** Discriminated Unions ***************************/


/**
 * Like the Corba discriminated union, this has a typed scalar that is
 * compared against the case labels to determine what the rest of the
 * data is.  So this is an aggregate data type consisting of the value
 * to be switched on followed by the element determined by this
 * value.  Unlike the Corba union, we also name the field holding the
 * value switched on. 
 *
 * The union as a whole creates a named nested scope for further name
 * definitions (as does module, struct, and interface), but the
 * individual case labels do not create further subscopes.
 */
union_dcl:
        tk_UNION name_def '{' {
	  GCPtr<Symbol> sym = Symbol::Create($2, lexer->isActiveUOC, sc_union);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- union identifier \"%s\" reused\n",
			 $2.loc.c_str(),
			 $2.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->docComment = lexer->grabDocComment();

	  /* Struct name is a scope for its formals: */
	  Symbol::PushScope(sym);
	}
        tk_SWITCH '(' switch_type /* name_def */ ')' '{'  {
#if 0
	  GCPtr<Symbol> sym = Symbol::Create($8, lexer->isActiveUOC, sc_switch);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- switch identifier \"%s\" reused\n",
		    $8.loc.c_str(),
		    $8.str.c_str());
	    num_errors++;
	    YYERROR;
	  }

	  sym->type = $7;
	  sym->docComment = lexer->grabDocComment();
#else
	  Symbol::CurScope->type = $7;
#endif

	  /* Struct name is a scope for its formals: */
	  /* Symbol::PushScope(sym); */
	}
        cases 
            '}' ';'
        '}' {
          SHOWPARSE("union_dcl -> tk_UNION name_def '{' tk_SWITCH '(' switch_type name_def ')' '{' cases '}'\n");
	  /* Symbol::PopScope(); */
	  Symbol::PopScope();
	  $$ = $2;
	}
 ;

/**
 * One may only switch non scalar types other than floating point.
 * Any objections?  We expect to initially support only small enough
 * subranges of these types that an array lookup implementation is
 * reasonable. Let's say 0..255.
 */
switch_type:
        integer_type            /* subranges of tk_INTEGER */ {
          SHOWPARSE("switch_type -> integer_type\n");
	  $$ = $1;
	}
 |      char_type               /* subranges of Unicode tk_WCHAR */ {
          SHOWPARSE("switch_type -> char_type\n");
	  $$ = $1;
	}
 |      tk_BOOLEAN                 /* TRUE or FALSE */ {
          SHOWPARSE("switch_type -> tk_BOOLEAN\n");
          $$ = Symbol::KeywordScope->LookupChild("#bool", 0);
        }
 |      scoped_name {           /* must name one of the other switch_types */
          SHOWPARSE("switch_type -> scoped_name\n");

	  $$ = Symbol::CreateRef($1, lexer->isActiveUOC);
	}
 ;

cases:  case {
          SHOWPARSE("cases -> case\n");
	}
 |      cases case {
          SHOWPARSE("cases -> cases case\n");
        }
 ;

/**
 * Each case consists of one or more case labels, zero or more name
 * definitions (scoped to the union as a whole), and one element
 * declaration.  (Note: I would like to scope these definitions to
 * the case, but the case has no natural name.)
 *
 * Note (5/2/2007): there is no compelling reason to allow case
 * definitions here, and it creates a scoping problem in the
 * symbol/AST construction, so I dropped the internal case_definitions
 * from the grammar. They weren't in CORBA in any case.
 */
case:   {
#if 0
          GCPtr<Symbol> sym = Symbol::GenSym(sc_caseScope, lexer->isActiveUOC);
	  Symbol::PushScope(sym);
#endif
        } case_labels element_dcl ';' {
          SHOWPARSE("case -> case_labels element_dcl\n");
#if 0
	  Symbol::PopScope();
#endif
        }
 ;

case_labels:
        case_label
   { SHOWPARSE("case_labels -> case_label\n"); }
 |      case_labels case_label
   { SHOWPARSE("case_labels -> case_labels case_label\n"); }
 ;

case_label:
        tk_CASE const_expr ':' {
	  GCPtr<Symbol> sym = 0;
          SHOWPARSE("case_label -> tk_CASE const_expr ':'\n");

	  std::string s = $2->v.bn.asString(10);
	  sym = Symbol::Create(LToken($2->loc, s), lexer->isActiveUOC, 
			       sc_caseTag);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- case identifier \"%s\" reused\n",
		    $2->loc.c_str(), s.c_str());
	    num_errors++;
	    YYERROR;
	  }
	}
 |      tk_DEFAULT ':' {
	  GCPtr<Symbol> sym = 0;
          SHOWPARSE("case_label -> tk_DEFAULT ':'\n");
	  sym = Symbol::Create(LToken($1.loc, "#default:"),
			       lexer->isActiveUOC, sc_caseTag);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- default case reused\n",
		    $1.loc.c_str());
	    num_errors++;
	    YYERROR;
	  }
	}
 ;

element_dcl:
        type name_def {
          GCPtr<Symbol> sym = 0;
          SHOWPARSE("element_dcl -> type name_def\n");
	  sym = Symbol::Create($2, lexer->isActiveUOC, sc_member);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- element identifier \"%s\" reused\n",
		    $2.loc.c_str(),
		    $2.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = $1;
	  sym->docComment = lexer->grabDocComment();
	}
 ;



/********************** Enums ***************************/


/**
 * Just as characters have character codes but the character is not
 * its character code (it is only represented by its character code),
 * so an enumerated type consists of a set of named enumerated values,
 * each of which happens to be represented by a unique integer.  This
 * declaration declares the name of the type and the names of the
 * values of that type.  <p>
 *
 * Are the value names scoped to the type name?  In order words, given
 * "enum Color { RED, GREEN, BLUE }", must one then say "Color::RED"
 * or simply "RED"?  What's Corba's answer? 
 */
/**
 * 
 */
enum_dcl:
        integer_type tk_ENUM name_def '{' {
	  GCPtr<Symbol> sym = Symbol::Create($3, lexer->isActiveUOC, sc_enum);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- enum identifier \"%s\" reused\n",
		    $3.loc.c_str(),
		    $3.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = $1;
	  sym->docComment = lexer->grabDocComment();
	  sym->v.bn = 0;    /* value for next enum member held here */
	  /* Enum name is a scope for its formals: */
	  Symbol::PushScope(sym);
	}
        enum_defs '}' {
          SHOWPARSE("integer_type enum_dcl -> tk_ENUM name_def '{' enum_defs '}'\n");
	  Symbol::PopScope();
	  $$ = $3;
	}
//  |     tk_ENUM '{' {
//	  GCPtr<Symbol> sym = Symbol::GenSym(sc_enum); /* anonymous */
//	  sym->docComment = lexer->grabDocComment();
//	  mpz_set_ui(sym->v.i, 0); /* value for next enum member held here */
//	  /* Enum name is a scope for its formals: */
//	  Symbol::PushScope(sym);
//	}
//        enum_defs '}' {
//          SHOWPARSE("enum_dcl -> tk_ENUM /* anonymous */ '{' enum_defs '}'\n");
//	  Symbol::PopScope();
//	}
 ;

enum_defs:
        name_def {
	  GCPtr<Symbol> theEnumTag = 0;
	  GCPtr<Symbol> sym = 0;

          SHOWPARSE("enum_defs -> name_def\n");
	  theEnumTag = Symbol::CurScope;
	  sym = Symbol::Create($1, lexer->isActiveUOC, sc_const);
	  if (!sym) {
	    fprintf(stderr, "%s:: syntax error -- econst identifier \"%s\" reused\n",
		    $1.loc.c_str(),
		    $1.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = Symbol::MakeIntLitFromBigNum($1.loc, theEnumTag->v.bn);
	  sym->type = Symbol::CurScope->type;
	  sym->docComment = lexer->grabDocComment();

	  /* Bump the enumeration's "next enumeral" value */
	  theEnumTag->v.bn = sym->value->v.bn + 1;
	}
 |      name_def '=' const_expr {
          GCPtr<Symbol> theEnumTag = 0;
	  GCPtr<Symbol> sym = 0;
          SHOWPARSE("enum_defs -> name_def '=' const_expr\n");
	  theEnumTag = Symbol::CurScope;
	  sym = Symbol::Create($1, lexer->isActiveUOC, sc_const);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- econst identifier \"%s\" reused\n",
		    $1.loc.c_str(),
		    $1.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
          sym->value = $3;
	  sym->type = Symbol::CurScope->type;
	  sym->docComment = lexer->grabDocComment();

	  /* Bump the enumeration's "next enumeral" value */
	  theEnumTag->v.bn = sym->value->v.bn + 1;
	}
 |      enum_defs ',' name_def {
          GCPtr<Symbol> theEnumTag = 0;
	  GCPtr<Symbol> sym = 0;
          SHOWPARSE("enum_defs -> enum_defs ',' name_def\n");
	  theEnumTag = Symbol::CurScope;
	  sym = Symbol::Create($3, lexer->isActiveUOC, sc_const);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- econst identifier \"%s\" reused\n",
		    $3.loc.c_str(),
		    $3.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = Symbol::MakeIntLitFromBigNum($3.loc, theEnumTag->v.bn);
	  sym->type = Symbol::CurScope->type;
	  sym->docComment = lexer->grabDocComment();

	  /* Bump the enumeration's "next enumeral" value */
	  theEnumTag->v.bn = sym->value->v.bn + 1;
        }
 |      enum_defs ',' name_def '=' const_expr {
          GCPtr<Symbol> theEnumTag = 0;
	  GCPtr<Symbol> sym = 0;
          SHOWPARSE("enum_defs -> enum_defs ',' name_def '=' const_expr\n");
	  theEnumTag = Symbol::CurScope;
	  sym = Symbol::Create($3, lexer->isActiveUOC, sc_const);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- econst identifier \"%s\" reused\n",
		    $3.loc.c_str(),
		    $3.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = $5;
	  sym->type = Symbol::CurScope->type;
	  sym->docComment = lexer->grabDocComment();

	  /* Bump the enumeration's "next enumeral" value */
	  theEnumTag->v.bn = sym->value->v.bn + 1;
        }
 ;

/********************** BitSets ***************************/


/**
 * A bitset is really an enumeration in sheep's clothing. For example,
 * it uses enum_defs internally.
 *
 * Rationale:
 *
 * Consider what happens when a variable or formal parameter is
 * declared as an enumerated type. When a value is passed or assigned,
 * some compilers will complain if they cannot determine that the
 * value is within the legal range of the enumerated type. One result
 * is that code like:
 *
 *    enum enum_t {
 *      one = 1,
 *      two = 2,
 *    };
 *    int foo(enum_t arg);
 *
 *    ****
 *
 *    foo(one|two);
 *
 * will not compile.
 *
 * Use bitset when what you intend is an unsigned argument that will
 * be the sum (logical or) of the enumeral values. Note that bitsets
 * are restricted to tk_ONLY be unsigned types.
 */

bitset_dcl:
          unsigned_integer_type tk_BITSET name_def '{' {
	  GCPtr<Symbol> sym = Symbol::Create($3, lexer->isActiveUOC, sc_enum);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- bitset identifier \"%s\" reused\n",
		    $3.loc.c_str(),
		    $3.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = $1;
	  sym->docComment = lexer->grabDocComment();
	  sym->v.bn = 0; /* value for next enum member held here */
	  /* Enum name is a scope for its formals: */
	  Symbol::PushScope(sym);
	}
        enum_defs '}' {
          SHOWPARSE("unsigned_integer_type bitset_dcl -> tk_BITSET name_def '{' enum_defs '}'\n");
	  Symbol::PopScope();
	  $$ = $3;
	}
//  |     tk_ENUM '{' {
//	  GCPtr<Symbol> sym = Symbol::GenSym(sc_enum); /* anonymous */
//	  sym->docComment = lexer->grabDocComment();
//	  mpz_set_ui(sym->v.bn, 0); /* value for next enum member held here */
//	  /* Enum name is a scope for its formals: */
//	  Symbol::PushScope(sym);
//	}
//        enum_defs '}' {
//          SHOWPARSE("enum_dcl -> tk_ENUM /* anonymous */ '{' enum_defs '}'\n");
//	  Symbol::PopScope();
//	}
 ;



/********************** Constant Declarations ***************************/
const_dcl:
        tk_CONST type name_def '=' const_expr {
	  GCPtr<Symbol> sym = 0;
          SHOWPARSE("const_dcl -> tk_CONST type name_def\n");
	  sym = Symbol::Create($3, lexer->isActiveUOC, sc_const);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- constant identifier \"%s\" reused\n",
		    $3.loc.c_str(),
		    $3.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = $5;
	  sym->type = $2;
	  sym->docComment = lexer->grabDocComment();

	  $$ = $3;
	}
 ;

/********************** Constant Expressions ***************************/


/**
 * Eventually, we expect to support the full set of operators that
 * Corba constant exressions support, although we will interpret these
 * operators in a precision unlimited fashion.  (It is not appropriate
 * for a language neutral framework to use a standard for arithmetic
 * other than mathematics, unless motivated by efficiency.  Since
 * constant expressions are evaluated at CapIDL compile time,
 * efficiency isn't an issue.)
 */
const_expr: const_sum_expr {
          SHOWPARSE("const_expr -> const_sum_expr\n");
	  $$ = $1;
	}
 ;

const_sum_expr:
        const_mul_expr {
          SHOWPARSE("const_sum_expr -> const_mul_expr\n");
	  $$ = $1;
        }
  |     const_mul_expr '+' const_mul_expr {
          SHOWPARSE("const_sum_expr -> const_mul_expr '*' const_mul_expr\n");

	  $$ = Symbol::MakeExprNode($2, $1, $3);
        }
  |     const_mul_expr '-' const_mul_expr {
          SHOWPARSE("const_sum_expr -> const_mul_expr '/' const_mul_expr\n");

	  $$ = Symbol::MakeExprNode($2, $1, $3);
        }
  ;

const_mul_expr:
        const_term {
          SHOWPARSE("const_mul_expr -> const_term\n");

          $$ = $1;
        }
  |     const_term '*' const_term {
          SHOWPARSE("const_expr -> const_mul_expr '*' const_mul_expr\n");

	  $$ = Symbol::MakeExprNode($2, $1, $3);
        }
  |     const_term '/' const_term {
          SHOWPARSE("const_expr -> const_mul_expr '/' const_mul_expr\n");

	  $$ = Symbol::MakeExprNode($2, $1, $3);
        }
  ;

const_term:
        scoped_name {
          SHOWPARSE("const_term -> scoped_name\n");

	  $$ = Symbol::CreateRef($1, lexer->isActiveUOC);
	}
|       literal {
          SHOWPARSE("const_term -> literal\n");
	  $$ = $1;
        }
 |      '(' const_expr ')' {
          SHOWPARSE("const_term -> '(' const_expr ')'\n");
          $$ = $2;
        }
 ;

/* |      tk_StringLiteral      { 
 *          SHOWPARSE("literal -> tk_StringLiteral\n");
 *          $$ = Symbol::MakeStringLit($1); 
 *        }
 */

literal:
        tk_IntegerLiteral     { 
          SHOWPARSE("literal -> tk_IntegerLiteral\n");
          $$ = Symbol::MakeIntLit($1); 
        }
 |      '-' tk_IntegerLiteral     { 
	  GCPtr<Symbol> left = 0;
	  GCPtr<Symbol> right = 0;
          SHOWPARSE("literal -> '-' tk_IntegerLiteral\n");

	  left = Symbol::MakeIntLit(LToken($1.loc, "0"));
	  right = Symbol::MakeIntLit($2);
	  $$ = Symbol::MakeExprNode($1, left, right);
        }
 |      tk_CharLiteral        { 
          SHOWPARSE("literal -> tk_CharLiteral\n");
          $$ = Symbol::MakeCharLit($1); 
        }
 |      tk_FloatingPtLiteral  { 
          SHOWPARSE("literal -> tk_FloatingPtLiteral\n");
          $$ = Symbol::MakeFloatLit($1); 
        }
 |      tk_MIN '(' integer_type ')' {
          GCPtr<Symbol> sym = 0;
          SHOWPARSE("literal -> tMin '(' integer_type ')'\n");
	  sym = Symbol::MakeMinLit($1.loc, $3);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- infinite integral types have no min/max\n",
		    $1.loc.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  $$ = sym;
        }
 |      tk_MAX '(' integer_type ')' {
          GCPtr<Symbol> sym = 0;
          SHOWPARSE("literal -> tMax '(' integer_type ')'\n");
          sym = Symbol::MakeMaxLit($1.loc, $3);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- infinite integral types have no min/max\n",
		    $1.loc.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  $$ = sym;
        }
 |      tk_TRUE  { 
          SHOWPARSE("literal -> tk_TRUE\n");
          $$ = Symbol::KeywordScope->LookupChild($1, 0); 
        }
 |      tk_FALSE {
          SHOWPARSE("literal -> tk_FALSE\n");
          $$ = Symbol::KeywordScope->LookupChild($1, 0); 
        }
 ;



/******************* Interfaces / Capabilities **********************/


/* SHAP: In CORBA, an interface is a scope. While it defines
 * operations, it can also define structures, constants, and so
 * forth. That is, all top-level declarables except for interfaces and
 * modules can be declared within an interface scope. Interfaces and
 * modules are distinguished in that the notion of "interface" in the
 * CORBA IDL is "top level" (ignoring modules, which are optional),
 * and so interfaces cannot contain interfaces.
 *
 * To implement this, I revised MarkM's original grammar to replace
 * his <message> nonterminal with an <twoway_dcl> (which is what the
 * CORBA 2.4.1 grammar called operation declarations) and
 * re-introduced the original style of <interface> nonterminal with
 * <if_definitions> and <if_definition> as intermediate nonterminals
 * in place of <messages>. In some ways this is regrettable, as it
 * renders the "what is a capability" question less clear -- an
 * interface can now contain stuff in addition to the operations.
 */

/**
 * Defines a capability type.  The type of a capability is defined by
 * what you can send it.  By far the most common convention is to send
 * it a message, which consists of an order code, a sequence of
 * capability arguments, and a sequence of pure data arguments.  (For
 * present purposes, it would be inappropriate to try to support
 * an argument type that mixed data and capabilities.)  One of the
 * capability arguments is also special -- the resume argument.  When
 * the invoker of a capability does an EROS CALL, the resume argument
 * position is filled in by the OS with a Resume key which, when
 * invoked, will cause the caller to continue with the arguments to
 * the Resume key invocation. <p>
 *
 * CapIDL supports three levels of description, from most convenient,
 * conventional, and high level, to most flexible and low level. <p>
 *
 * The lowest level description is the "struct level", in which a
 * capability is defined as a one argument procedure with no return,
 * where the argument describes the capabilities and data that may be
 * passed to that capability.  At this level, there are not
 * necessarily any resume parameters.  However, when CALLing such a
 * capability, the OS generated Resume key will be passed in the first
 * capability parameter position.  At this level, there are not
 * necessarily any order codes.  However, the type will often by a
 * discriminated union switching on an enum, in which case the values
 * of this enum are the moral equivalent of order codes.  The struct
 * level is the only context in CapIDL where structs and unions can mix
 * data and capabilities. <p>
 *
 * Next is the "oneway message level" or just the "oneway level".
 * Here, a capability is described by explicitly declaring the
 * separate messages you can send to the capability.  This level
 * expands to the struct level by turning each message name into an
 * enum value (of an enum type specific to this capability type),
 * gathering all the arguments for each message in order into a
 * struct, and gathering all these enum values and argument structs
 * into one big discriminated union.  At this level, order codes are
 * implicit and built in, but are still not necessarily any resume
 * parameters. <p>
 *
 * Next is the "twoway message level" or just the "twoway level".
 * Here, a capability is still described as a set of messages, but any
 * one of these messages may instead be declared as twoway.  At the
 * twoway level, the resume parameter is implicit and built in as
 * well.  Instead, tk_OUT parameters and a list of RAISEd Exceptions is
 * added to the oneway message declaration.  These effectively declare
 * the type of the Resume parameter, at the price of imposing some
 * conventional restrictions.  Specifically, the Resume parameter type
 * may only have one success order code, and all other order codes
 * must either be well known system exception codes, or must pass only
 * an Exception (which will be one of those in the tk_RAISES clause).
 * The twoway level expands into the oneway level by turning these
 * extra declarations into an explicit type on an explicit Resume
 * parameter. 
 */
interface_dcl:
        tk_INTERFACE name_def if_extends {
	  GCPtr<Symbol> sym = 0;
#if 0
	  fprintf(stderr, "Attempting create of interface %s\n", $2.ltok.c_str());
#endif
	  sym = Symbol::Create($2, lexer->isActiveUOC, sc_interface);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- interface identifier \"%s\" reused\n",
		    $2.loc.c_str(),
		    $2.str.c_str());
	    num_errors++;
	    YYERROR;
	  }

	  if ($3)
	    sym->baseType = $3;

	  sym->docComment = lexer->grabDocComment();

	  /* Procedure name is a scope for its formals: */
	  Symbol::PushScope(sym);
        }
        raises '{' if_definitions '}'  { // message-levels
          SHOWPARSE("interface_dcl -> tk_INTERFACE name_def if_extends raises '{' if_definitions '}'\n");
	  Symbol::PopScope();
        }
 |      tk_ABSTRACT tk_INTERFACE name_def if_extends {
          GCPtr<Symbol> sym = 0;
#if 0
	  fprintf(stderr, "Attempting create of interface %s\n", $3.ltok.c_str());
#endif
	  sym = Symbol::Create($3, lexer->isActiveUOC, sc_absinterface);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- interface identifier \"%s\" reused\n",
		    $3.loc.c_str(),
		    $3.str.c_str());
	    sym = Symbol::CurScope->LookupChild($3, 0); 
	    fprintf(stderr, "%s: First defined as %s \"%s\"\n",
		    $3.loc.c_str(),
		    sym->ClassName().c_str(),
		    sym->QualifiedName('_').c_str());
	    num_errors++;
	    YYERROR;
	  }

	  if ($4)
	    sym->baseType = $4;

	  sym->docComment = lexer->grabDocComment();

	  /* Procedure name is a scope for its formals: */
	  Symbol::PushScope(sym);
        }
        raises '{' if_definitions '}'  { // message-levels
          SHOWPARSE("interface_dcl -> tk_ABSTRACT tk_INTERFACE name_def if_extends raises '{' if_definitions '}'\n");
	  Symbol::PopScope();
        }
 ;

if_extends: 
    /* empty */ {
      SHOWPARSE("if_extends -> <empty>\n");
      $$ = 0; 
    }
  | tk_EXTENDS dotted_name {
      SHOWPARSE("if_extends -> tk_EXTENDS dotted_name\n");

      $$ = Symbol::CreateRef($2, lexer->isActiveUOC);
    }
 ;

if_definitions:
    /* empty */ { }
 |      if_definitions if_definition {
          SHOWPARSE("if_definitions -> if_definitions if_definition\n");
        }
 ;

if_definition:
        struct_dcl ';'
   { SHOWPARSE("if_definition -> struct_dcl ';'\n"); }
 |      except_dcl ';'
   { SHOWPARSE("if_definition -> except_dcl ';'\n"); }
 |      union_dcl  ';'
   { SHOWPARSE("if_definition -> union_dcl ';'\n"); }
 |      namespace_dcl  ';'
   { SHOWPARSE("if_definition -> namespace_dcl ';'\n"); }
 |      enum_dcl ';'
   { SHOWPARSE("if_definition -> enum_dcl ';'\n"); }
 |      bitset_dcl ';'
   { SHOWPARSE("if_definition -> bitset_dcl ';'\n"); }
 |      typedef_dcl ';'
   { SHOWPARSE("if_definition -> typedef_dcl ';'\n"); }
 |      const_dcl ';'
   { SHOWPARSE("if_definition -> const_dcl ';'\n"); }
 |      opr_dcl ';'
   { SHOWPARSE("if_definition -> opr_dcl ';'\n"); }
 |      repr_dcl ';'
   { SHOWPARSE("if_definition -> repr_dcl ';'\n"); }
 ;

/**
 * The semantics here is tricky, because the function symbol MUST be
 * created before any of the parameter symbols. This is necessary
 * because the function symbol forms a scope into which the parameters
 * must be installed.
 *
 * It is a nuisance that we have to enumerate the opr_qual
 * and non-oper_qual cases, but failing to do so causes a shift-reduce
 * conflict with enum declarations. */
opr_dcl:
        tk_INHERITS name_def {
          SHOWPARSE("opr -> tk_INHERITS name_def\n");
	  // Make it an inherited operation provisionally.
	  GCPtr<Symbol> sym = 
	    Symbol::Create($2, lexer->isActiveUOC, sc_inhOperation);
	  sym->docComment = lexer->grabDocComment();
	}
 |      opr_qual tk_ONEWAY tk_VOID name_def '(' {
	  GCPtr<Symbol> sym = Symbol::Create($4, lexer->isActiveUOC, sc_oneway);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- operation identifier \"%s\" reused\n",
		    $4.loc.c_str(),
		    $4.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->flags = $1;
	  sym->type = Symbol::VoidType;
	  sym->docComment = lexer->grabDocComment();
	  /* Procedure name is a scope for its formals: */
	  Symbol::PushScope(sym);
	}
        params ')' {
          SHOWPARSE("opr -> tk_ONEWAY tk_VOID name_def '(' params ')'\n");
	  Symbol::PopScope();
	}
 |      tk_ONEWAY tk_VOID name_def '(' {
	  GCPtr<Symbol> sym = Symbol::Create($3, lexer->isActiveUOC, sc_oneway);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- operation identifier \"%s\" reused\n",
		    $3.loc.c_str(),
		    $3.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = Symbol::VoidType;
	  sym->docComment = lexer->grabDocComment();
	  /* Procedure name is a scope for its formals: */
	  Symbol::PushScope(sym);
	}
        params ')' {
          SHOWPARSE("opr -> tk_ONEWAY tk_VOID name_def '(' params ')'\n");
	  Symbol::PopScope();
	}
 |      opr_qual ret_type name_def '(' {
	  GCPtr<Symbol> sym = Symbol::Create($3, lexer->isActiveUOC, sc_operation);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- operation identifier \"%s\" reused\n",
		    $3.loc.c_str(),
		    $3.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->flags = $1;
	  sym->type = $2;
	  sym->docComment = lexer->grabDocComment();
	  /* Procedure name is a scope for its formals: */
	  Symbol::PushScope(sym);
	}
	param_2s ')' { 
        } raises {
	  Symbol::PopScope();
          SHOWPARSE("opr -> ret_type name_def '(' param_2s ')' raises\n");
	}
 |      ret_type name_def '(' {
	  GCPtr<Symbol> sym = Symbol::Create($2, lexer->isActiveUOC, sc_operation);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- operation identifier \"%s\" reused\n",
		    $2.loc.c_str(),
		    $2.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = $1;
	  sym->docComment = lexer->grabDocComment();
	  /* Procedure name is a scope for its formals: */
	  Symbol::PushScope(sym);
	}
	param_2s ')' { 
        } raises {
	  Symbol::PopScope();
          SHOWPARSE("opr -> ret_type name_def '(' param_2s ')' raises\n");
	}
 ;

opr_qual:
        tk_NOSTUB {
          SHOWPARSE("opr_qual -> tk_NOSTUB\n");
          $$ = SF_NOSTUB;
        }
 |      tk_CLIENT {
          SHOWPARSE("opr_qual -> tk_CLIENT\n");
          $$ = SF_NO_OPCODE|SF_NOSTUB;
        }
 ;
       
/**
 * In expanding to the struct level, all the params are gathered into
 * a struct.  First come all the capability arguments, and then all
 * the pure data parameters.  A param cannot be of mixed data and
 * capability type.
 */
params:
        /*empty*/ {
          SHOWPARSE("params -> <empty>\n");
        }
 |      param_list {
          SHOWPARSE("params -> param_list\n");
        }
 ;

param_list:
        param {
          SHOWPARSE("param_list -> param\n");
        }
 |      param_list ',' param {
          SHOWPARSE("param_list -> param_list ',' param\n");
	}
 ;

/**
 * The parameter name is defined within the scope of this message
 * name.  In this one regard, each individual named message is also a
 * named nested scope.  This allows, for example, a following tk_REPR
 * clause to give placement advice on a parameter by refering to it as
 * "messageName::parameterName" 
 *
 * In a discussion about hash generation, MarkM and I decided at one
 * point to make the formal parameter name optional, because it is not
 * semantically significant. The problem with making it optional is
 * that it breaks doc comments that refer to parameters by name.
 */
param:
        param_type name_def {
          GCPtr<Symbol> sym = 0;
          SHOWPARSE("param -> type name_def\n");

	  sym = Symbol::Create($2, lexer->isActiveUOC, sc_formal);
	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- parameter identifier \"%s\" reused\n",
		    $2.loc.c_str(),
		    $2.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = $1;
	  sym->docComment = lexer->grabDocComment();

	  $$ = sym;
	}
 ;

/**
 * In expanding to the oneway level, a non-tk_VOID return type becomes a
 * first tk_OUT argument of the same type, and the message is left with a
 * tk_VOID return type.  The generated out parameter will be named
 * "_result", and so tk_REPR advice can refer to the result by this name
 * even when the return type syntax is used.
 */
ret_type:
        type {
          SHOWPARSE("ret_type -> type\n");
	  $$ = $1;
        }
 |      tk_VOID {
          SHOWPARSE("ret_type -> tk_VOID\n");
	  $$ = Symbol::VoidType;
	}
 ;

param_2s:
        /*empty*/ {
          SHOWPARSE("param_2s -> <empty>\n");
        }
 |      param_2_list {
          SHOWPARSE("param_2s -> param_2_list\n");
        }
 ;

param_2_list:
        param_2 {
          SHOWPARSE("param_2_list -> param_2\n");
        }
 |      param_2_list ',' param_2 {
          SHOWPARSE("param_2_list -> param_2_list ',' param_2\n");
        }
 ;

/**
 * In expanding to the oneway level, a normal (tk_IN) parameter is left
 * alone, but the out parameters are gathered together to form the
 * normal parameter list of the oneway success message to the Resume
 * parameter. 
 */
param_2:
        param {
          SHOWPARSE("param_2 -> param\n");
        }
 |      tk_OUT param {
          SHOWPARSE("param_2 -> tk_OUT param\n");
	  $2->cls = sc_outformal;
	}
 ;

/**
 * In expanding to the oneway level, each Exception listed in the
 * tk_RAISES clause becomes a separate oneway problem reporting message
 * on the Resume parameter type, whose argument is just that
 * exception.  Should the order codes for these problem messages come
 * from the Exception declaration or the tk_RAISES clause?  The first
 * would seem to make more sense, except there's no natural way to
 * coordinate uniqueness.  How do EROS or KeyKOS currently assign
 * order codes for reporting problems?
 */
raises:
 /*empty*/ {
          SHOWPARSE("raises -> <empty>\n");
        }
 |      tk_RAISES '(' exceptions ')' {
          SHOWPARSE("raises -> tk_RAISES '(' exceptions ')'\n");
        }
 ;

/* SHAP: Make the exceptions clause require at least one name, as the
 * entire clause can be eliminated if no exceptions are to be raised.
 */
exceptions:
        scoped_name {
          GCPtr<Symbol> sym = 0;
          SHOWPARSE("exceptions -> scoped_name\n");

	  sym = Symbol::CreateRaisesRef($1, lexer->isActiveUOC);

	  if (!sym) {
	    fprintf(stderr, "%s: syntax error -- exception \"%s\" already raised\n",
		    $1.loc.c_str(),
		    $1.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
        }
 |      exceptions ',' scoped_name {
          GCPtr<Symbol> sym = 0;
          SHOWPARSE("exceptions -> exceptions ',' scoped_name\n");

	  sym = Symbol::CreateRaisesRef($3, lexer->isActiveUOC);

	  if (!sym) {
	    fprintf(stderr, 
		    "%s: syntax error -- exception \"%s\" already raised\n",
		    $3.loc.c_str(),
		    $3.str.c_str());
	    num_errors++;
	    YYERROR;
	  }
        }
 ;




/********************** Representation ***************************/


/**
 * A placeholder for representation advice, including placement
 * advice.  Because, by Corba scoping rules, names in subscopes can be
 * refered to using paths, tk_REPR advice could appear anywhere in the
 * compilation unit (ie, source file) after the declaration of the
 * thing being advised.  (Is there a possible security problem with
 * this?)  However, good style is for the tk_REPR advice to follow as
 * closely as possible the declarations it is advising. <p>
 *
 * The reason the rest of the spec was careful never to refer to bit
 * representation, but rather speaks in terms of subranges (except for
 * floating point, where it's unavoidable), is that the rest of the
 * spec is only about semantics, not representation.  The tk_REPR advice can
 * therefore be only about representation, not semantics. <p>
 *
 * Conflicting advice, or advice that specifies a representation not
 * able to preserve the semantics (such as insufficient bits for a
 * given subrange) must be caught and reported statically, and must
 * cause a failure to compile. <p>
 *
 * In the absence of advice, default advice applies.  This is
 * appropriate for human written source, but is a poor way for
 * programs to speak to other programs.  Instead, there needs to be a
 * tool for turning source capidl files into fully-advised capidl
 * files.  The fully advised files are likely to be canonicalized in
 * other ways as well.  These will be written once and read many
 * times, so the goal is to make it easier on the reading program.
 * The getAllegedType query may even return a fast binary equivalent
 * to a fully advised and somewhat canonicalized capIDL file.
 */
repr_dcl:
        tk_REPR '{' advisories '}' {
          SHOWPARSE("repr_dcl -> tk_REPR '{' advisories '}'\n");
        }
 |      tk_REPR advice {
          SHOWPARSE("repr_dcl -> tk_REPR advice\n");
        }
 ;

advisories:
        /*empty*/ {
          SHOWPARSE("advisories -> <empty>\n");
        }
 |      advisories advice {
          SHOWPARSE("advisories -> advisories advice\n");
        }
 ;

/**
 * The side to the right of the colon says how the thing named on the
 * left side should be represented, and where it should be placed.
 * This production is currently a placeholder until we figure out what
 * kind of advice we'd like to express.  As we figure this out, expect
 * the right side to grow. <p><pre>
 *
 * Some plausible meanings:
 *      enum_value: integer     // defines this enum value to be this integer
 *      message_name: integer   // gives the message this order code
 *      a_wstring: "UTF-8"      // represents the wide string in UTF-8
 *      a_wstring: "UTF-16"     // represents the wide string in UTF-16
 *      a_module: "LITTLE_ENDIAN" // inherited unless overridden?
 * </pre>
 */
advice:
        scoped_name ':' const_expr ';' {
          SHOWPARSE("advice -> scoped_name ':' const_expr ';'\n");
	}
 ;


%%

