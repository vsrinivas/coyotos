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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <dirent.h>

#include <string>
#include <iostream>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include <libsherpa/sha1.hxx>
#include "SymTab.hxx"
#include "util.hxx"

/** Following scopes are universal, in the sense that they are 
    shared across all input processing. The /UniversalScope/ is the
    containing scope for all of the package scopes we will be
    building. Each imported package in turn has a top-level package
    scope.
*/
/** Universal package scope.
    
    This scope object contains all of the packages that are loaded in
    a given execution of capidl. The general processing strategy of
    capidl is to process all of the input units of compilation and
    then export header/implementation files (according to the command
    line options) for each package found under UniversalScope at the
    end of processing. 
*/
    
GCPtr<Symbol> Symbol::UniversalScope = 0;

/*** 
     Scope for builtin types. 

     Several of the symbols here are internally defined type symbols
     that are proxy objects for the builtin types. All of these
     symbols begin with "#", in order to ensure that they will not
     match any of the builtin lookup predicates.

     The keyword scope probably ought to be replaced by a CapIDL
     package. It then becomes an interesting question whether builtin
     type names such as int, long should be overridable.
 */
GCPtr<Symbol> Symbol::KeywordScope = 0; /* scope for keywords */ 
GCPtr<Symbol> Symbol::VoidType = 0;	/* void type symbol */ 
GCPtr<Symbol> Symbol::CurScope = 0;

const char *Symbol::sc_names[] = {
#define SYMCLASS(pn, x, n) #x,
#include "symclass.def"
#undef  SYMCLASS
};

const char *Symbol::sc_printnames[] = {
#define SYMCLASS(pn, x,n) pn,
#include "symclass.def"
#undef  SYMCLASS
};

unsigned Symbol::sc_isScope[] = {
#define SYMCLASS(pn, x,n) n,
#include "symclass.def"
#undef  SYMCLASS
};

void
Symbol::PushScope(GCPtr<Symbol> newScope)
{
  assert (!Symbol::CurScope || newScope->nameSpace == Symbol::CurScope);

  Symbol::CurScope = newScope;
}

void
Symbol::PopScope()
{
  assert(Symbol::CurScope);

  Symbol::CurScope->complete = true;

  Symbol::CurScope = Symbol::CurScope->nameSpace;
}

GCPtr<Symbol> 
Symbol::LookupChild(const std::string& nm, GCPtr<Symbol> bound)
{
  std::string::size_type dot;
  GCPtr<Symbol> child = 0;
  GCPtr<Symbol> childScope = 0;
  std::string ident;
  std::string rest;

  /* If /nm/ is not a qualified name, then it should be an immediate
     descendant: */

  dot = nm.find('.');

  if (dot == std::string::npos) {
    for (size_t i = 0; i < children.size(); i++) {
      child = children[i];

      if (bound && (bound == child))
	return 0;

      if (child->name == nm)
	return child;
    }

    return 0;
  }
  else {
    ident = nm.substr(0, dot);
    rest = nm.substr(dot+1);

    childScope = LookupChild(ident, bound);
    if (childScope) 
      return childScope->LookupChild(rest, 0);
    return 0;
    
  }
}

// Ensure that all of the pointer fields get initialized:
Symbol::Symbol()
{
  nameSpace = 0;
  type = 0;
  baseType = 0;
  value = 0;
  docComment = 0;
}

GCPtr<Symbol> 
Symbol::Construct(const LToken& tok,
		  bool isActiveUOC, SymClass sc)
{
  GCPtr<Symbol> s = new Symbol;

  s->loc = tok.loc;
  s->name = tok.str;
  s->cls = sc;
  s->ifDepth = 0;		/* depth 0 arbitrarily reserved */
  s->v.bn = 0;
  s->v.lty = lt_void;
  s->flags = 0;
  s->isActiveUOC = isActiveUOC;

  s->mark = false;

  s->complete = Symbol::sc_isScope[sc] ? true : false;

  return s;
}

GCPtr<Symbol> 
Symbol::Create_inScope(const LToken& tok, 
		       bool isActiveUOC, SymClass sc, GCPtr<Symbol> scope)
{
  GCPtr<Symbol> sym = 0;
  
  if (scope) {
    sym = scope->LookupChild(tok, 0);

    if (sym)
      return 0;
  }
  
  sym = Symbol::Construct(tok, isActiveUOC, sc);

  if (scope) {
    sym->nameSpace = scope;
    scope->AddChild(sym);
  }

  return sym;
}

GCPtr<Symbol> 
Symbol::Create(const LToken& tok, 
	       bool isActiveUOC, SymClass sc)
{
  return Symbol::Create_inScope(tok, isActiveUOC, sc, Symbol::CurScope);
}

GCPtr<Symbol> 
Symbol::CreateRef_inScope(const LToken& tok, 
			  bool isActiveUOC, 
			  GCPtr<Symbol> inScope)
{
  GCPtr<Symbol> sym = Symbol::Construct(tok, isActiveUOC, sc_symRef);
  sym->type = 0;
  sym->nameSpace = inScope;

  return sym;
}

GCPtr<Symbol> 
Symbol::CreateRef(const LToken& tok, bool isActiveUOC)
{
  return Symbol::CreateRef_inScope(tok, isActiveUOC, Symbol::CurScope);
}

GCPtr<Symbol> 
Symbol::CreateRaisesRef_inScope(const LToken& tok, 
				bool isActiveUOC, GCPtr<Symbol> inScope)
{
  GCPtr<Symbol> sym = 0;
  GCPtr<Symbol> raises = 0;

  assert(inScope);
  assert(inScope == Symbol::CurScope);	// check for usage error in the grammar

  /* If this exception has already been added, do not add it again. */

  /* FIX: What about raises inheritance in method overrides? */

  for (size_t i = 0; i < inScope->raises.size(); i++) {
    raises = inScope->raises[i];

    if (raises->name == tok.str)
      return 0;
  }

  sym = Symbol::Construct(tok, isActiveUOC, sc_symRef);
  sym->type = 0;
  sym->nameSpace = inScope;

  /* The reference per se does not have a namespace */
  inScope->raises.append(sym);

  return sym;
}

GCPtr<Symbol> 
Symbol::CreateRaisesRef(const LToken& tok, 
			bool isActiveUOC)
{
  return Symbol::CreateRaisesRef_inScope(tok, isActiveUOC, Symbol::CurScope);
}

/**
 * Because of the namespace design of the IDL language, it is possible
 * for one interface A to publish type ty (thus A.ty) and for a second
 * interface B to contain an operator that makes reference to this
 * type by specifying it as "A.ty". This usage is perfectly okay, but
 * in order to output the necessary header file dependencies it is
 * necessary that the generated compilation unit for B import the
 * generated compilation unit for A.
 *
 * Catch 1: The B unit of compilation may internally define a
 * namespace A that *also* defines a type ty. If so, this definition
 * "wins". We therefore need to do symbol resolution 
 *
 * Catch 2: An interface can "extend" another interface. Thus. when
 * resolving name references within an interface we need to consider
 * names published by "base" interfaces.
 *
 * Catch 3: Package names have no inherent lexical ordering. In
 * consequence, units of compilation also have no inherent lexical
 * ordering.
 *
 * Therefore, our symbol reference resolution strategy must proceed as
 * follows:
 * 
 *   1. Perform a purely lexical lookup looking for a name resolution
 *      that lexically preceeds the name we are attempting to
 *      resolve.
 *
 *      The last (top) scope we will consider is the children of the
 *      unit of compilation (and the unit of compilation itself, of
 *      course). 
 *
 *      During the course of lexical resolution of interfaces, we will
 *      (eventually) traverse base interfaces. It is not immediately
 *      clear (to me) how much of the lexical context of a base
 *      interface is inherited, so THIS IS NOT CURRENTLY IMPLEMENTED.
 *
 *   2. If no resolution has been achieved by (1), attempt a top-level
 *      name resolution relative to the universal scope. Note that
 *      this resolution may result in a circular dependency between
 *      units of compilation.
 */
GCPtr<Symbol> 
Symbol::LexicalLookup(const std::string& nm, GCPtr<Symbol> bound)
{
  GCPtr<Symbol> sym = 0;
  GCPtr<Symbol> scope = this;

  while (scope) {
    /* It is wrong to use the /bound/ when the scope symbol is an
       sc_package symbol, because units of compilation are not
       necessarily introduced into their containing package in lexical
       order.

       A circular dependency at this level must be caught in the
       circular dependency check between units of compilation that is
       performed as a separate pass. */
    if (scope->cls == sc_package)
      bound = 0;

    sym = scope->LookupChild(nm, bound);
    if (sym)
      break;

    /* If the current scope is an interface, it may be an extension of
       some other interface, in which case the lexical scope continues
       into the parent interface. 

       Note that this logic only works because we guarantee resolution
       of baseType before attempting to resolve children in
       Symbol::ResolveReferences(). */
    if ( scope->baseType ) {
      GCPtr<Symbol> ifScope = scope->baseType;

      while (ifScope) {
#if 0
	fprintf(stderr, "%s: Resolving ref to \"%s\"\n",
		loc.c_str(), ifScope->name.c_str());
#endif

	ifScope = ifScope->ResolveRef();

	sym = ifScope->LookupChild(nm, 0);
	if (sym)
	  break;

	ifScope = ifScope->baseType;
      }

      if (sym)
	break;
    }

    bound = scope;
    scope = scope->nameSpace;

    /* Packages aren't really lexically nested, so do not consider
       metapackage a lexical scope: */
    if (scope && bound &&
	(scope->cls == sc_package) &&
	(bound->cls == sc_package))
      scope = 0;
  }

  return sym;
}

bool
Symbol::ResolveSymbolReference()
{
  assert (cls == sc_symRef);

  GCPtr<Symbol> sym = nameSpace->LexicalLookup(name, 0);

  if (!sym)
    sym = Symbol::UniversalScope->LookupChild(name, 0);

  if (!sym)
    fprintf(stderr, "%s: Unable to resolve symbol \"%s\"\n",
	    loc.c_str(), name.c_str());

  value = sym;

  return value ? true : false;
}

bool
Symbol::ResolveInheritedReference()
{
  assert (cls == sc_inhOperation || cls == sc_inhOneway);
  assert(nameSpace->IsInterface());

  if (type)
    return true;

  GCPtr<Symbol> scope = nameSpace->baseType;
  GCPtr<Symbol> sym = 0;

  while (scope) {
    scope = scope->ResolveRef();

    sym = scope->LookupChild(name, 0);
    if (sym)
      break;

    scope = scope->baseType;
  }

  if (!sym) {
    fprintf(stderr, "%s: Unable to resolve inherited symbol \"%s\"\n",
	    loc.c_str(), name.c_str());
    return false;
  }

  if (sym->cls != sc_oneway && sym->cls != sc_operation) {
    fprintf(stderr, "%s: Inherit keyword can only inherit operations and oneway methods.\n",
	    loc.c_str(), name.c_str());
    return false;
  }

  // The front end provisionally marks all inherited symbols as
  // sc_inhOperation. That may be wrong. If so, update it here.
  if (sym->cls == sc_oneway)
    cls = sc_inhOneway;

  type = sym->type;
  nameSpace = sym->nameSpace;

  Symbol::PushScope(this);
  for (size_t i = 0; i < sym->children.size(); i++) {
    GCPtr<Symbol> inhArg = sym->children[i];
    GCPtr<Symbol> arg = Symbol::Create(inhArg->name, this->isActiveUOC, inhArg->cls);
    arg->type = inhArg->type;
    arg->docComment = inhArg->docComment;
  }
  for (size_t i = 0; i < sym->raises.size(); i++) {
    GCPtr<Symbol> inhRaises = sym->raises[i];
    GCPtr<Symbol> raises = 
      Symbol::CreateRaisesRef(inhRaises->name, this->isActiveUOC);
  }
  Symbol::PopScope();

  return true;
}

GCPtr<Symbol> 
Symbol::CreatePackage(const LToken& tok, GCPtr<Symbol> inPkg)
{
  GCPtr<Symbol> thePkg = 0;
  std::string::size_type dot;

  dot = tok.str.find('.');

  if (dot == std::string::npos) {
    /* We are down to the tail identifier. */
    thePkg = inPkg->LookupChild(tok, 0);
    if (thePkg) {
      if (thePkg->cls == sc_package)
	return thePkg;
      return 0;
    }
    else
      return Symbol::Create_inScope(tok, false, sc_package, inPkg);
  }
  else {
    const std::string ident = tok.str.substr(0, dot);
    const std::string rest = tok.str.substr(dot+1);

    inPkg = Symbol::CreatePackage(LToken(tok.loc, ident), inPkg);
    if (!inPkg)
      return inPkg;		// fail

    return Symbol::CreatePackage(LToken(tok.loc, rest), inPkg);
  }
}

GCPtr<Symbol> 
Symbol::GenSym(SymClass sc, bool isActiveUOC)
{
  return Symbol::GenSym_inScope(sc, isActiveUOC, Symbol::CurScope);
}

GCPtr<Symbol> 
Symbol::GenSym_inScope(SymClass sc, bool isActiveUOC, GCPtr<Symbol> inScope)
{
  static int gensymcount = 0;
  char buf[20];
  sprintf(buf, "#anon%d", gensymcount++);

  return Symbol::Create_inScope(LToken(buf),
				isActiveUOC, sc, inScope);
}

GCPtr<Symbol> 
Symbol::MakeIntLit(const LToken& tok)
{
  GCPtr<Symbol> sym = Symbol::Construct(tok, true, sc_value);
  sym->v.lty = lt_integer;
  sym->v.bn = BigNum(tok.str.c_str());

  return sym;
}

GCPtr<Symbol> 
Symbol::MakeMinLit(const LexLoc& loc, GCPtr<Symbol> s)
{
  assert(s->cls == sc_primtype);
  assert(s->v.lty == lt_integer || 
	 s->v.lty == lt_unsigned || 
	 s->v.lty == lt_char);

  if (s->v.lty == lt_integer) {
    if (s->v.bn == 8)
      return Symbol::MakeIntLit(LToken(loc, "-128"));
    else if (s->v.bn == 16)
      return Symbol::MakeIntLit(LToken(loc, "-32768"));
    else if (s->v.bn == 32)
      return Symbol::MakeIntLit(LToken(loc, "-2147483648"));
    else if (s->v.bn == 64)
      return Symbol::MakeIntLit(LToken(loc, "-9223372036854775808"));
  }
  else if (s->v.lty == lt_unsigned) {
    // Unsigned integers and all character types:
    return Symbol::MakeIntLit(LToken(loc, "0"));
  }

  // FIX: Character types should return character literals

  return 0;			/* no min/max */
}

GCPtr<Symbol> 
Symbol::MakeMaxLit(const LexLoc& loc, GCPtr<Symbol> s)
{
  assert(s->cls == sc_primtype);
  assert(s->v.lty == lt_integer || 
	 s->v.lty == lt_unsigned || 
	 s->v.lty == lt_char);
  /* Max char lits not supported yet! */

  if (s->v.lty == lt_integer) {
    if (s->v.bn == 8)
      return Symbol::MakeIntLit(LToken(loc, "127"));
    else if (s->v.bn == 16)
      return Symbol::MakeIntLit(LToken(loc, "32767"));
    else if (s->v.bn == 32)
      return Symbol::MakeIntLit(LToken(loc, "2147483647"));
    else if (s->v.bn == 64)
      return Symbol::MakeIntLit(LToken(loc, "9223372036854775807"));
  }
  else if (s->v.lty == lt_unsigned) {
    if (s->v.bn == 8)
      return Symbol::MakeIntLit(LToken(loc, "255"));
    else if (s->v.bn == 16)
      return Symbol::MakeIntLit(LToken(loc, "65535"));
    else if (s->v.bn == 32)
      return Symbol::MakeIntLit(LToken(loc, "4294967295"));
    else if (s->v.bn == 64)
      return Symbol::MakeIntLit(LToken(loc, "18446744073709551615"));
  }

  // FIX: Character types should return character literals
#if 0
  // FIX: There is a serious encoding problem here. These need to come
  // out as character literals.
  if ( s->name == "#wchar8" )
    return Symbol::MakeIntLit(LToken(loc, "255"));
  if ( s->name == "#wchar16" )
    return Symbol::MakeIntLit(LToken(loc, "65535"));
  if ( s->name == "#wchar32" )
    return Symbol::MakeIntLit(LToken(loc, "4294967295"));
#endif

  return 0;			/* no min/max */
}

GCPtr<Symbol> 
Symbol::MakeIntLitFromBigNum(const LexLoc& loc, const BigNum& bn)
{
  GCPtr<Symbol> sym = Symbol::Construct(LToken(loc, bn.asString()), true, sc_value);
  sym->v.lty = lt_integer;
  sym->v.bn = bn;

  return sym;
}

GCPtr<Symbol> 
Symbol::MakeStringLit(const LToken& tok)
{
  GCPtr<Symbol> sym = Symbol::Construct(tok, true, sc_value);
  sym->v.lty = lt_string;

  return sym;
}

GCPtr<Symbol> 
Symbol::MakeFloatLit(const LToken& tok)
{
  GCPtr<Symbol> sym = Symbol::Construct(tok, true, sc_value);
  sym->v.lty = lt_float;
  sym->v.d = strtod(tok.str.c_str(), NULL);

  return sym;
}

GCPtr<Symbol> 
Symbol::MakeCharLit(const LToken& tok)
{
  GCPtr<Symbol> sym = Symbol::Construct(tok, true, sc_value);
  sym->v.lty = lt_char;
  sym->v.bn = BigNum(tok.str);

  return sym;
}

GCPtr<Symbol> 
Symbol::MakeKeyword(const std::string& nm, 
		    SymClass sc,
		    LitType lt,
		    unsigned value)
{
  GCPtr<Symbol> sym = Symbol::Construct(LToken(nm),
					false, sc);
  sym->nameSpace = Symbol::KeywordScope;
  sym->v.lty = lt;
  sym->v.bn = value;
  Symbol::KeywordScope->AddChild(sym);

  return sym;
}

GCPtr<Symbol> 
Symbol::MakeExprNode(const LToken& op,
		     GCPtr<Symbol> left, 
		     GCPtr<Symbol> right)
{
  GCPtr<Symbol> sym = Symbol::Construct(op, false, sc_arithop);
  sym->nameSpace = Symbol::UniversalScope;
  sym->v.lty = lt_char;
  sym->v.bn = BigNum(op.str[0]);

  sym->children.append(left);
  sym->children.append(right);

  return sym;
}

static GCPtr<Symbol> int_types[9] = {
  0,				/* int<0> */
  0,				/* int<8> */
  0,				/* int<16> */
  0,				/* int<24> */
  0,				/* int<32> */
  0,				/* int<40> */
  0,				/* int<48> */
  0,				/* int<56> */
  0,				/* int<64> */
};

static GCPtr<Symbol> uint_types[9] = {
  0,				/* uint<0> */
  0,				/* uint<8> */
  0,				/* uint<16> */
  0,				/* uint<24> */
  0,				/* uint<32> */
  0,				/* uint<40> */
  0,				/* uint<48> */
  0,				/* uint<56> */
  0,				/* uint<64> */
};

GCPtr<Symbol> 
Symbol::LookupIntType(unsigned bitsz, bool uIntType)
{
  if (bitsz % 8)
    return 0;
  if (bitsz > 64)
    return 0;
  
  bitsz /= 8;

  return uIntType ? uint_types[bitsz] : int_types[bitsz];
}

void
Symbol::InitSymtab()
{
  GCPtr<Symbol> sym = 0;
  GCPtr<Symbol> boolType = 0;

  Symbol::UniversalScope = Symbol::Construct(LToken("<UniversalScope>"), false, sc_scope);
  Symbol::KeywordScope = Symbol::Construct(LToken("<KeywordScope>"), false, sc_scope);

  /* Primitive types. These all start with '.' to ensure that they
   * cannot be successfully matched as an identifier, since the
   * "names" of these types are purely for my own convenience in doing
   * "dump" operations on the symbol table.
   */

  // Booleans are encoded in 8 bits.
  boolType = Symbol::MakeKeyword("#bool", sc_primtype, lt_bool, 8);

  Symbol::MakeKeyword("#int", sc_primtype, lt_integer, 0);
  Symbol::MakeKeyword("#int8", sc_primtype, lt_integer, 8);
  Symbol::MakeKeyword("#int16", sc_primtype, lt_integer, 16);
  Symbol::MakeKeyword("#int32", sc_primtype, lt_integer, 32);
  Symbol::MakeKeyword("#int64", sc_primtype, lt_integer, 64);

  Symbol::MakeKeyword("#uint8", sc_primtype, lt_unsigned, 8);
  Symbol::MakeKeyword("#uint16", sc_primtype, lt_unsigned, 16);
  Symbol::MakeKeyword("#uint32", sc_primtype, lt_unsigned, 32);
  Symbol::MakeKeyword("#uint64", sc_primtype, lt_unsigned, 64);

  Symbol::MakeIntLit(LToken("0"));	/* min unsigned anything */

  Symbol::MakeIntLit(LToken("127")); /* max pos signed 8 bit */
  Symbol::MakeIntLit(LToken("-128")); /* max neg signed 8 bit */ 
  Symbol::MakeIntLit(LToken("255")); /* max unsigned 8 bit */
    
  Symbol::MakeIntLit(LToken("32767")); /* max pos signed 16 bit */
  Symbol::MakeIntLit(LToken("-32768")); /* max neg signed 16 bit */ 
  Symbol::MakeIntLit(LToken("65535")); /* max unsigned 16 bit */
    
  Symbol::MakeIntLit(LToken("2147483647")); /* max pos signed 32 bit */
  Symbol::MakeIntLit(LToken("-2147483648")); /* max neg signed 32 bit */ 
  Symbol::MakeIntLit(LToken("4294967295")); /* max unsigned 32 bit */
    
  Symbol::MakeIntLit(LToken("9223372036854775807")); /* max pos signed 64 bit */
  Symbol::MakeIntLit(LToken("-9223372036854775808")); /* max neg signed 64 bit */ 
  Symbol::MakeIntLit(LToken("18446744073709551615")); /* max unsigned 64 bit */
    
  Symbol::MakeKeyword("#float32", sc_primtype, lt_float, 32);
  Symbol::MakeKeyword("#float64", sc_primtype, lt_float, 64);
  Symbol::MakeKeyword("#float128", sc_primtype, lt_float, 128);

  Symbol::MakeKeyword("#wchar8", sc_primtype, lt_char, 8);
  Symbol::MakeKeyword("#wchar16", sc_primtype, lt_char, 16);
  Symbol::MakeKeyword("#wchar32", sc_primtype, lt_char, 32);

  Symbol::MakeKeyword("#wstring8", sc_primtype, lt_string, 8);
  Symbol::MakeKeyword("#wstring32", sc_primtype, lt_string, 32);

  Symbol::VoidType = Symbol::MakeKeyword("#void", sc_primtype, lt_void, 0);

  sym = Symbol::MakeKeyword("true", sc_builtin, lt_bool, 1);
  sym->type = boolType;

  sym = Symbol::MakeKeyword("false", sc_builtin, lt_bool, 0);
  sym->type = boolType;

  sym->type = boolType;
}

GCPtr<Symbol> 
Symbol::FindPackageScope()
{
  GCPtr<Symbol> scope = Symbol::CurScope;
  
  while (scope && (scope->cls != sc_package))
    scope = scope->nameSpace;

  return scope;
}

std::string
symname_join(const std::string& n1, const std::string& n2, char sep)
{
  return n1 + sep + n2;
}

GCPtr<Symbol> 
Symbol::ResolveType()
{
  if (IsBasicType())
    return this;
      
  switch(cls) {
  case sc_formal:
  case sc_outformal:
  case sc_member:
  case sc_operation:
  case sc_oneway:
  case sc_inhOperation:
  case sc_inhOneway:
    return this;
    
  case sc_typedef:
    return type->ResolveType();
  case sc_symRef:
    return ResolveRef()->ResolveType();
  case sc_exception:
    return this;
  default:
    return 0;
  }
}

std::string
Symbol::QualifiedName(char sep)
{
  std::string nm;
  // If asked for the qualified name of a symbol reference, give
  // the qualified name of the referenced symbol:
  GCPtr<Symbol> sym = this->ResolveRef();

  if (sym->IsAnonymous())
    return "";

  nm = sym->name;

  while (sym->nameSpace && (sym->nameSpace != Symbol::UniversalScope)) {
    sym = sym->nameSpace;
    if (!sym->IsAnonymous())
      nm = symname_join(sym->name, nm, sep);
  }

  return nm;
}

unsigned long long
Symbol::CodedName()
{
  std::string s = QualifiedName('.');

  OpenSHA *sha = new OpenSHA();

  sha->append(s);
  sha->finish();

  return sha->signature64();
}

#ifdef SYMDEBUG
void
Symbol::QualifyNames()
{
  if (cls == sc_symRef || cls == sc_value || cls == sc_builtin)
    return;			// skip these!

  qualifiedName = QualifiedName('.');

  if (baseType)
    baseType->QualifyNames();
  if (type)
    type->QualifyNames();
  if (value)
    value->QualifyNames();

  for(size_t i = 0; i < children.size(); i++)
    children[i]->QualifyNames();

  for(size_t i = 0; i < raises.size(); i++)
    raises[i]->QualifyNames();
}
#endif

bool
Symbol::ResolveReferences()
{
  bool result = true;

#if 0
  fprintf(stderr, "%s: Resolving references on symbol \"%s\" ty=%s\n",
	  loc.c_str(), name.c_str(), ClassName().c_str());
#endif

  /* In the usual case, we resolve the sc_symRef to the proper
     value, but rely on the rest of the scope traversals to deal with
     any recursive cases. See, however, the comment on baseType. */
  if (cls == sc_symRef)
    return ResolveSymbolReference();

  if (cls == sc_inhOperation || cls == sc_inhOneway)
    return ResolveInheritedReference();

  if (cls == sc_value || cls == sc_builtin)
    return true;

  /* It is imperative that the baseType (if any) be resolved before
     any child resolutions are attempted, because lexical lookups to
     resolve the children may require traversal of the basetype.

     The baseType reference is almost certainly an sc_symRef, so to
     satisfy this requirement we need to traverse through that to
     propagate resolution properly. */
  if (baseType) {
    // First call to get the value field of the sc_symRef filled in:
    result = result && baseType->ResolveReferences();

    // Now propagate resolution through to the true interface if
    // necessary:
    if (baseType->cls == sc_symRef) {
      baseType = baseType->ResolveRef();
      result = result && baseType->ResolveReferences();
    }
  }
  if (type)
    result = result && type->ResolveReferences();
  if (value)
    result = result && value->ResolveReferences();

  for(size_t i = 0; i < children.size(); i++)
    result = result && children[i]->ResolveReferences();

  for(size_t i = 0; i < raises.size(); i++)
    result = result && raises[i]->ResolveReferences();

  return result;
}

void
Symbol::ResolveIfDepth()
{
  if (baseType)
    baseType->ResolveIfDepth();

  if (cls == sc_interface || cls == sc_absinterface) {
    if (ifDepth)		/* non-zero indicates already resolved */
      return;

    ifDepth = baseType ? baseType->ifDepth + 1 : 1;
  }

  if (type)
    type->ResolveIfDepth();
  if (value) {
    value->ResolveIfDepth();
    ifDepth = value->ifDepth;
  }

  for(size_t i = 0; i < children.size(); i++)
    children[i]->ResolveIfDepth();

  for(size_t i = 0; i < raises.size(); i++)
    raises[i]->ResolveIfDepth();
}

bool
Symbol::TypeCheck()
{
  GCPtr<Symbol> sym = this;
  bool result = true;

  if (baseType && ! baseType->IsInterface()) {
    fprintf(stderr,
	    "%s: Interface \"%s\" extends \"%s\", "
	    "which is not an interface type\n", 
	    sym->loc.c_str(),
	    sym->QualifiedName('.').c_str(),
	    sym->baseType->name.c_str());
    result = false;
  }

#if 0
  if (sym->type && Symbol::IsType(sym->type, sc_seqType)) {
    fprintf(stderr, 
	    "%s: %s \"%s\" specifies sequence type  \"%s\" (%s), "
	    "which is not yet supported\n", 
	    sym->loc.c_str(),
	    Symbol::ClassName(sym).c_str(),
	    sym->QualifiedName('.').c_str(),
	    sym->type->name.c_str(),
	    Symbol::ClassName(sym->type).c_str());
    result = false;
  }
#endif

  if (sym->value && ! sym->value->IsConstantValue()) {
    fprintf(stderr, "%s: Symbol \"%s\" is not a constant value\n",
	    sym->loc.c_str(),
	    sym->value->name.c_str());
    result = false;
  }

  if (sym->raises.size() > 0) {
    if (!sym->IsInterface() && !sym->IsOperation()) {
      fprintf(stderr, "%s: Exceptions can only be raised by interfaces and methods\n",
	      sym->loc.c_str(),
	      sym->name.c_str());
      result = false;
    }

    for (size_t i = 0; i < sym->raises.size(); i++) {
      GCPtr<Symbol> excpt = sym->raises[i];

      assert(excpt->cls == sc_symRef);

      if (! excpt->IsException()) {
	fprintf(stderr, 
		"%s: %s \"%s\" raises  \"%s\" (%s), "
		"which is not an exception type\n", 
		sym->loc.c_str(),
		sym->ClassName().c_str(),
		sym->QualifiedName('.').c_str(),
		excpt->QualifiedName('_').c_str(),
		excpt->ClassName().c_str());
	result = false;
      }
    }
  }

  if (sym->cls == sc_formal && !sym->type->IsValidParamType()) {
    fprintf(stderr,
	    "%s: %s \"%s\" has illegal parameter type "
	    "(hint: sequence<> and buffer<> should be typedefed)\n", 
	    sym->loc.c_str(),
	    sym->ClassName().c_str(),
	    sym->QualifiedName('.').c_str());
    result = false;
  }

  if (sym->cls == sc_outformal && !sym->type->IsValidParamType()) {
    fprintf(stderr, 
	    "%s: %s \"%s\" has illegal parameter type "
	    "(hint: sequence<> and buffer<> should be typedefed)\n", 
	    sym->loc.c_str(),
	    sym->ClassName().c_str(),
	    sym->QualifiedName('.').c_str());
    result = false;
  }

  if (sym->cls == sc_member && !sym->type->IsValidMemberType()) {
    fprintf(stderr, 
	    "%s: %s \"%s\" has an illegal type "
	    "(buffers cannot be structure/unino members))\n", 
	    sym->loc.c_str(),
	    sym->ClassName().c_str(),
	    sym->QualifiedName('.').c_str());
    result = false;
  }

  if (sym->cls == sc_seqType && !sym->type->IsValidSequenceBaseType()) {
    fprintf(stderr, 
	    "%s: %s \"%s\" has an illegal base type "
	    "(sequences of buffers make no sense))\n", 
	    sym->loc.c_str(),
	    sym->ClassName().c_str(),
	    sym->QualifiedName('.').c_str());
    result = false;
  }

  if (sym->cls == sc_bufType && !sym->type->IsValidBufferBaseType()) {
    fprintf(stderr, 
	    "%s: %s \"%s\" has an illegal base type "
	    "(buffers of sequences are currently not permitted))\n", 
	    sym->loc.c_str(),
	    sym->ClassName().c_str(),
	    sym->QualifiedName('.').c_str());
    result = false;
  }

  if (sym->cls == sc_symRef)
    return result;

  for(size_t i = 0; i < sym->children.size(); i++)
    result = result && sym->children[i]->TypeCheck();

#if 0
  // All of the raises symbols are going to be sc_symRef to some
  // exception that is declared somewhere within an existing
  // interface. We have validated above that they are in fact
  // sc_symRef, and that the symrefs do point to something that is an
  // exception. We rely on the usual case child traversal to type
  // check the exception definition when we traverse its containing
  // interface.
  for(size_t i = 0; i < sym->raises.size(); i++)
    result = result && sym->raises[i]->TypeCheck();
#endif

  return result;
}

GCPtr<Symbol> 
Symbol::UnitOfCompilation()
{
  GCPtr<Symbol> sym = this;

  if (sym->cls == sc_package)
    return 0;
  if (!sym->nameSpace)
    return 0;
  if (sym->nameSpace->cls == sc_package)
    return sym;
  return sym->nameSpace->UnitOfCompilation();
}

#if 0
bool
Symbol::ResolveDepth()
{
  int myDepth = 0;

  if (cls == sc_value || cls == sc_builtin || cls == sc_primtype) {
    depth = 0;
    return true;
  }

  /* A depth value of -1 signals a symbol whose depth resolution is in
     progress. If we are asked to resolve the depth of such a symbol,
     there is a circular dependency. */

  if (depth == -1) {
    if (cls != sc_symRef) 
      fprintf(stderr, 
	      "%s: Symbol \"%s\"\n", loc.c_str(), QualifiedName('.'));
    return false;
  }

  depth = -1;			// unknown until otherwise proven

  if (baseType && !baseType->ResolveDepth()) {
    if (cls != sc_symRef)
      fprintf(stderr, "%s: Symbol \"%s\"\n", loc.c_str(), QualifiedName('.'));
    return false;
  }
  if (type && !type->IsReferenceType() && !type->ResolveDepth()) {
    if (cls != sc_symRef)
      fprintf(stderr, "%s: Symbol \"%s\"\n", loc.c_str(), QualifiedName('.'));
    return false;
  }
  if (value && !value->ResolveDepth()) {
    if (cls != sc_symRef)
      fprintf(stderr, "%s: Symbol \"%s\"\n", loc.c_str(), QualifiedName('.'));
    return false;
  }

  if (baseType) myDepth = max(myDepth, baseType->depth);
  if (type && !type->IsReferenceType()) myDepth = max(myDepth, type->depth);
  if (value) myDepth = max(myDepth, value->depth);

  for(GCPtr<Symbol> child = children; child; child = child->next) {
    if (!child->ResolveDepth()) {
      if (cls != sc_symRef)
	fprintf(stderr, "%s: Symbol \"%s\"\n", 
		loc.c_str(), QualifiedName('.'));
      return false;
    }

    myDepth = max(myDepth, child->depth);
  }

  depth = myDepth + 1;

  return true;
}

void
Symbol::ClearDepth()
{
  if (depth == -2)
    return;

  depth = -2;

  if (baseType) baseType->ClearDepth();
  if (type) type->ClearDepth();
  if (value) value->ClearDepth();

  for(GCPtr<Symbol> child = children; child; child = child->next)
    child->ClearDepth();
}
#endif

bool
Symbol::IsLinearizable()
{
  GCPtr<Symbol> sym = this;

  if (sym->mark) {
    fprintf(stderr,
	    "%s: Symbol \"%s\"\n", 
	    sym->loc.c_str(), sym->QualifiedName('.').c_str());
    return false;
  }

  sym->mark = true;

  if (sym->baseType && !sym->baseType->IsLinearizable()) {
    if (sym->cls != sc_symRef) 
      fprintf(stderr,
	      "%s: Symbol \"%s\" extends \"%s\"\n", 
	      sym->loc.c_str(), sym->QualifiedName('.').c_str(),
	      sym->baseType->QualifiedName('.').c_str());
    goto fail;
  }
    
  if (sym->type && 
      !sym->type->IsReferenceType() && 
      !sym->type->IsLinearizable()) {
    if (sym->cls != sc_symRef) 
      fprintf(stderr, 
	      "%s: Symbol \"%s\" type is \"%s\"\n", 
	      sym->loc.c_str(), sym->QualifiedName('.').c_str(),
	      sym->type->QualifiedName('.').c_str());
    goto fail;
  }

  if (sym->value && !sym->value->IsLinearizable())
    goto fail;

  for(size_t i = 0; i < sym->children.size(); i++) {
    if (!sym->children[i]->IsLinearizable()) {
      if (sym->cls != sc_symRef) 
	fprintf(stderr, 
		"%s: Symbol \"%s\" contains \"%s\"\n", 
		sym->loc.c_str(), sym->QualifiedName('.').c_str(),
		sym->children[i]->QualifiedName('.').c_str());
      goto fail;
    }
  }

  for(size_t i = 0; i < sym->raises.size(); i++) {
    if (!sym->raises[i]->IsLinearizable()) {
      if (sym->cls != sc_symRef) 
	fprintf(stderr,
		"%s: Symbol \"%s\" contains \"%s\"\n", 
		sym->loc.c_str(), sym->QualifiedName('.').c_str(),
		sym->raises[i]->QualifiedName('.').c_str());
      goto fail;
    }
  }

  sym->mark = false;

  return true;

 fail:
  sym->mark = false;

  return false;
}

void
Symbol::ClearAllMarks()
{
  mark = false;

  if (baseType)
    baseType->ClearAllMarks();
  if (type)
    type->ClearAllMarks();
  if (value && cls != sc_symRef)
    value->ClearAllMarks();

  for(size_t i = 0; i < children.size(); i++)
    children[i]->ClearAllMarks();

  for(size_t i = 0; i < raises.size(); i++)
    raises[i]->ClearAllMarks();
}

void 
Symbol::ComputeDependencies(SymVec& depVec)
{
  UniversalScope->ClearAllMarks();
  this->DoComputeDependencies(depVec);
}

void 
Symbol::DoComputeDependencies(SymVec& depVec)
{
  GCPtr<Symbol> sym = this->ResolveRef();

  if (sym->mark)
    return;

  sym->mark = true;

  //  std::cerr << "Checking " << sym->name 
  // << " (" << sym->ClassName() << ")\n";

  GCPtr<Symbol> uoc = sym->UnitOfCompilation();
  if (!uoc)
    return;

  if (!depVec.contains(uoc)) {
    std::cerr << QualifiedName('.') << " depends on " << uoc->QualifiedName('.') << '\n';
    depVec.append(uoc);
  }

  if (sym->baseType)
    sym->baseType->DoComputeDependencies(depVec);

  if (sym->type)
    sym->type->DoComputeDependencies(depVec);

  if (sym->value)
    sym->value->DoComputeDependencies(depVec);
    
  for(size_t i = 0; i < sym->children.size(); i++)
    sym->children[i]->DoComputeDependencies(depVec);

  for(size_t i = 0; i < sym->raises.size(); i++)
    sym->raises[i]->DoComputeDependencies(depVec);
}

#if 0
void 
Symbol::ComputeTransDependencies(SymVec& depVec)
{
  GCPtr<Symbol> sym = this;

  if (sym->cls == sc_primtype)
    return;

  if (sym->cls == sc_symRef)
    sym = sym->ResolveRef();

  GCPtr<Symbol> targetUoc = sym->UnitOfCompilation();
  if (!depVec.contains(targetUoc)) {
    depVec.append(targetUoc);
    targetUoc->ComputeTransDependencies(depVec);
  }

  if (sym->baseType)
    sym->baseType->ComputeTransDependencies(depVec);

  if (sym->type)
    sym->type->ComputeTransDependencies(depVec);

  if (sym->value)
    sym->value->ComputeTransDependencies(depVec);
    
  for(size_t i = 0; i < sym->children.size(); i++)
    sym->children[i]->ComputeTransDependencies(depVec);

  for(size_t i = 0; i < sym->raises.size(); i++)
    sym->raises[i]->ComputeTransDependencies(depVec);
}
#endif

int
Symbol::CompareByName(GCPtr<Symbol>  const *s1, GCPtr<Symbol>  const *s2)
{
  /* two depths are the same */
  return (*s1)->name.compare((*s2)->name);
}

int
Symbol::CompareByQualifiedName(GCPtr<Symbol>  const *s1, GCPtr<Symbol>  const *s2)
{
  /* two depths are the same */
  return (*s1)->QualifiedName('_').compare((*s2)->QualifiedName('_'));
}

bool
Symbol::IsFixedSerializable()
{
  GCPtr<Symbol> sym = this;

  bool result = true;

  sym = sym->ResolveType();
  
  if (sym->IsReferenceType())
    return true;

  if (sym->type && ! sym->type->IsFixedSerializable())
    return false;

  switch(sym->cls) {
  case sc_primtype:
    {
      if (sym->v.lty == lt_string)
	return false;
      if (sym->v.lty == lt_integer && sym->v.bn == 0)
	return false;
      break;
    }

  case sc_seqType:
  case sc_bufType:
    return false;

  case sc_enum:
  case sc_bitset:
    return true;

  default:
    break;
  }
  
  for(size_t i = 0; i < sym->children.size(); i++) {
    GCPtr<Symbol> child = sym->children[i];
    if (!child->IsTypeSymbol())
      result = result && child->type->IsFixedSerializable();
  }

  for(size_t i = 0; i < sym->raises.size(); i++)
    result = result && sym->raises[i]->IsFixedSerializable();

  return result;
}

bool
Symbol::IsDirectSerializable()
{
  GCPtr<Symbol> sym = this;

  return sym->IsFixedSerializable();
}

extern BigNum compute_value(GCPtr<Symbol> s);

/**
 * Compute the alignment requirements for the direct portion of the
 * type.
 *
 * @bug The alignment requirements for integral and floating types
 * are, in theory, wrong, but the algorithm below yields the correct
 * answer on all of the machines we know baout.
 */
unsigned
Symbol::alignof(const ArchInfo& archInfo)
{
  GCPtr<Symbol> s = this;

  switch(s->cls) {
  case sc_primtype:
    {
      switch(s->v.lty) {
      case lt_integer:
      case lt_unsigned:
      case lt_char:
      case lt_bool:
	{
	  /* Integral types are aligned to their size, not to exceed
	     wordSize. */

	  unsigned bits = s->v.bn.as_uint32();
	  unsigned align = bits / 8;
	  if (archInfo.worstIntAlign < align) 
	    align = archInfo.worstIntAlign;

	  return align;
	}
      case lt_float:
	{
	  /* Floating types are aligned to their size, not to exceed
	     worstFloatAlign. */

	  unsigned bits = s->v.bn.as_uint32();
	  unsigned natAlign = bits / 8;
	  unsigned worstAlign = archInfo.floatByteAlign;
	  return (worstAlign < natAlign) ? worstAlign : natAlign;
	}
      case lt_string:
	{
	  unsigned bits = s->v.bn.as_uint32();
	  return bits/8;
	}
      case lt_void:
	return 1;
      }
      break;
    }
  case sc_enum:
  case sc_bitset:
    {
      // FIX: I suspect strongly that this is wrong, but I am
      // following C convention here.
      return archInfo.wordBytes;
      break;
    }
  case sc_struct:
    {
      unsigned align = 0;

      /* Alignment of struct is alignment of worst member. */
      for(size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];
	if (!child->IsTypeSymbol())
	  align = max(align, child->type->alignof(archInfo));
      }
      return align;
    }
  case sc_seqType:
  case sc_bufType:
    {
      /* Alignment of the vector struct: */
      return archInfo.wordBytes;
    }
  case sc_typedef:
  case sc_member:
  case sc_formal:
  case sc_outformal:
  case sc_arrayType:
    {
      return s->type->alignof(archInfo);
    }

  case sc_union:
    {
      unsigned align = type->alignof(archInfo);

      /* Alignment of union is alignment of worst member, including
	 union tag field. */
      for(size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];
	align = max(align, child->type->alignof(archInfo));
      }

      return align;
    }
  case sc_interface:
  case sc_absinterface:
    {
      assert(false);
      return 0;
    }
  case sc_symRef:
    {
      return s->value->alignof(archInfo);
    }
  default:
    {
      fprintf(stderr,
	      "%s: Alignment of unknown type (class %s) for symbol \"%s\"\n", 
	      s->loc.c_str(),
	      s->ClassName().c_str(),
	      s->QualifiedName('.').c_str());
      exit(1);
      break;
    }
  }

  return 0;
}

#if 0
unsigned
Symbol::sizeof(GCPtr<Symbol> s)
{
  unsigned len = 0;

  switch(s->cls) {
  case sc_primtype:
    {
      switch(s->v.lty) {
      case lt_integer:
      case lt_unsigned:
      case lt_char:
      case lt_bool:
      case lt_float:
	{
	  unsigned bits = s->v.bn.as_uint32();
	  return bits/8;
	}
      case lt_string:
	{
	  fprintf(stder, "Strings are not yet supported \"%s\"\n", 
		  Symbol::QualifiedName(s, '.'));
	  exit(1);
	}
	break;
      case lt_void:
	assert(false);
	break;
      }

      break;
    }

  case sc_enum:
  case sc_bitset:
    {
      // FIX: I suspect strongly that this is wrong, but I am
      // following C convention here.
      return ENUMERAL_SIZE;
    }
  case sc_struct:
    {
      for(size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];
	if (!Symbol::IsType(child)) {
	  len = round_up(len, Symbol::alignof(child));
	  len += Symbol::sizeof(child->type);
	}
      }

      /* Need to round up to next struct alignment boundary for array
	 computationions to be correct: */
      len = round_up(len, Symbol::alignof(s));
      return len;
    }
  case sc_typedef:
  case sc_member:
  case sc_formal:
  case sc_outformal:
    {
      return Symbol::sizeof(s->type);
    }
  case sc_seqType:
  case sc_bufType:
    {
      /* Vector structure itself is aligned at a 4-byte boundary,
	 contains a 4-byte max, a 4-byte length and a 4-byte pointer
	 to the members. Need to reserve space for that. */
      return TARGET_LONG_SIZE * 3;

#if 0
      offset = round_up(offset, TARGET_LONG_SIZE);
      offset += (2 * TARGET_LONG_SIZE);

      /* This is a bit tricky, as the size of the element type and the
	 alignment of the element subtype may conspire to yield a
	 "hole" at the end of each vector element. */
      BigNum bound = compute_value(s->value);
      unsigned align = s->type->alignof();
      unsigned elemSize = 0;
      Symbol::estimateSize(s->type, elemSize);
      elemSize = round_up(elemSize, align);

      mpz_t total;
      total = bound * elemSize;
      offset += total.as_uint32();
      break;
#endif
    }
  case sc_arrayType:
    {
      /* This is a bit tricky, as the size of the element type and the
	 alignment of the element subtype may conspire to yield a
	 "hole" at the end of each vector element. */
      BigNum bound = compute_value(s->value);

      unsigned align = s->type->alignof();
      unsigned elemSize = Symbol::sizeof(s->type);
      elemSize = round_up(elemSize, align);

      mpz_t total;
      mpz_init(total);
      mpz_mul_ui(total, &bound, elemSize);
      return mpz_get_ui(total);
      break;
    }
  case sc_union:
    {
      assert(false);
      break;
    }
  case sc_interface:
  case sc_absinterface:
    {
      assert(false);
      break;
    }
  case sc_symRef:
    {
      return Symbol::sizeof(s->value);
      break;
    }
  default:
    {
      fprintf(stder, 
	      "Size computation of unknown type for symbol \"%s\"\n", 
	      Symbol::QualifiedName(s, '.'));
      exit(1);
    }
  }

  return 0;
}
#endif

/**
 * Compute the conservative worst-case number of inline bytes
 * associated with this type.
 *
 * Unions are not supported at present. This is a bug.
 */
unsigned
Symbol::directSize(const ArchInfo& archInfo)
{
  GCPtr<Symbol> s = this;

  size_t len = 0;

  switch(s->cls) {
  case sc_primtype:
    {
      switch(s->v.lty) {
      case lt_integer:
      case lt_unsigned:
      case lt_char:
      case lt_bool:
      case lt_float:
	{
	  unsigned bits = s->v.bn.as_uint32();
	  return bits/8;
	}
      case lt_string:
	{
	  fprintf(stderr, 
		  "%s: Strings are not yet supported \"%s\"\n", 
		  s->loc.c_str(),
		  s->QualifiedName('.').c_str());
	  exit(1);
	}
      case lt_void:
	return 0;
      }

      break;
    }

  case sc_enum:
  case sc_bitset:
    {
      // FIX: I suspect strongly that this is wrong, but I am
      // following C convention here.
      return archInfo.wordBytes;
    }
  case sc_struct:
    {
      for(size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];
	if (child->IsTypeSymbol())
	  continue;

	len = round_up(len, child->alignof(archInfo));
	len += child->type->directSize(archInfo);
      }

      /* Need to round up to next struct alignment boundary for array
	 computationions to be correct: */
      len = round_up(len, s->alignof(archInfo));
      return len;
    }
  case sc_typedef:
  case sc_member:
  case sc_formal:
  case sc_outformal:
    {
      return s->type->directSize(archInfo);
    }
  case sc_seqType:
  case sc_bufType:
    {
      /* Vector structure itself is aligned at a 4-byte boundary,
	 contains a 4-byte max, a 4-byte length and a 4-byte pointer
	 to the members. Need to reserve space for that. */
      if (archInfo.wordBytes == 4)
	return 12;
      else if (archInfo.wordBytes == 8)
	return 16;
      else {
	fprintf(stderr, "Unhandled target word byte size %d for seqType/bufType\n", 
		archInfo.wordBytes);
	exit(1);
      }
    }
  case sc_arrayType:
    {
      /* This is a bit tricky, as the size of the element type and the
	 alignment of the element subtype may conspire to yield a
	 "hole" at the end of each vector element. */
      BigNum bound = compute_value(s->value);

      unsigned align = s->type->alignof(archInfo);
      unsigned elemSize = s->type->directSize(archInfo);
      elemSize = round_up(elemSize, align);

      {
	BigNum total;
	total = bound  * elemSize;
	return total.as_uint32();
      }
    }
  case sc_union:
    {
      // Union is a structure consisting of initial type tag followed
      // by variants. The type of the type tag is given by ->type.

      len = round_up(len, type->alignof(archInfo));
      len += type->directSize(archInfo);

      /* Round up to structure boundary: */
      size_t case_align = 0;
      for (size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];
	if (child->IsTypeSymbol())
	  continue;

	case_align = max(case_align, child->alignof(archInfo));
      }

      len = round_up(len, case_align);

      size_t longestLeg = 0;

      // Direct size of a union is maximal direct size of its legs,
      // allowing for alignment.

      for(size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];
	if (child->IsTypeSymbol())
	  continue;

	size_t legLen = 0;
	legLen = round_up(legLen, child->alignof(archInfo));
	legLen += child->type->directSize(archInfo);

	if (legLen > longestLeg)
	  longestLeg = legLen;
      }

      len += longestLeg;

      /* Need to round up to next struct alignment boundary for array
	 computationions to be correct: */
      len = round_up(len, s->alignof(archInfo));
      return len;
    }
  case sc_interface:
  case sc_absinterface:
    {
      assert(false);
      break;
    }
  case sc_symRef:
    {
      return s->value->directSize(archInfo);
      break;
    }
  default:
    {
      fprintf(stderr, 
	      "%s: Size computation of unknown type for symbol \"%s\"\n", 
	      s->loc.c_str(),
	      s->QualifiedName('.').c_str());
      exit(1);
    }
  }

  return 0;
}

/**
 * Compute the conservative worst-case number of non-inline bytes
 * associated with this type. At present we are unable to deal with
 * types of truly dynamic length.
 *
 * CapIDL differentiates three kinds of sequence types:
 *
 * - Arrays have statically fixed size and have no indirect component.
 * - Sequences have arbitrary size and an indirect component.
 * - Buffers have arbitrary size and an indirect component.
 *
 * Buffers may only appear as formal parameter types. They differ from
 * sequences in the way that the length parameter is specified. This
 * allows us to specify POSIX-like read/write in an API-compatible
 * way.
 *
 * The CapIDL language permits specification of dynamic sequence and
 * buffer sizes. The implementation only supports declarations that
 * supply a statically known upper bound. For these cases,
 * indirectSize() gives a @b conservative estimate that can be used
 * for worst-case buffer allocation.
 */
unsigned
Symbol::indirectSize(const ArchInfo& archInfo)
{
  GCPtr<Symbol> s = this;

  unsigned len = 0;

  switch(s->cls) {
  case sc_primtype:
  case sc_enum:
  case sc_bitset:
    return 0;

  case sc_struct:
    {
      for(size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];
	if (child->IsTypeSymbol())
	  continue;

	len += child->type->indirectSize(archInfo);
      }

      return len;
    }
  case sc_typedef:
  case sc_member:
  case sc_formal:
  case sc_outformal:
    {
      return s->type->indirectSize(archInfo);
    }
  case sc_arrayType:
    {
      return 0;			/* for now */
    }

  case sc_seqType:
  case sc_bufType:
    {
      /* This is a bit tricky, as the size of the element type and the
	 alignment of the element subtype may conspire to yield a
	 "hole" at the end of each vector element.
      
	 Eventually, this will be further complicated by the need to
	 deal with indirect types containing indirect types. At the
	 moment it just doesn't deal with this case at all. */
      BigNum bound = compute_value(s->value);

      unsigned align = s->type->alignof(archInfo);
      unsigned elemSize = s->type->directSize(archInfo);
      elemSize = round_up(elemSize, align);

      {
	BigNum total;
	total = bound * elemSize;
	return total.as_uint32();
      }
    }
  case sc_union:
    {
      // Indirect size of a union is maximal indirect size of its
      // legs, allowing for alignment.

      for(size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];
	if (child->IsTypeSymbol())
	  continue;

	size_t legLen = child->type->indirectSize(archInfo);

	if (legLen > len)
	  len = legLen;
      }

      return len;
    }
  case sc_interface:
  case sc_absinterface:
    {
      return 0;
      break;
    }
  case sc_symRef:
    {
      return s->value->indirectSize(archInfo);
    }
  default:
    {
      fprintf(stderr,
	      "%s: Size computation of unknown type for symbol \"%s\"\n", 
	      s->loc.c_str(),
	      s->QualifiedName('.').c_str());
      exit(1);
    }
  }

  return 0;
}

unsigned
Symbol::CountChildren(bool (Symbol::*predicate)())
{
  GCPtr<Symbol> s = this;

  unsigned count = 0;

  for (size_t i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> child = s->children[i];

    if (((*child).*predicate)())
      count++;
  }

  return count;
}

GCPtr<Symbol> 
Symbol::ResolvePackage()
{
  GCPtr<Symbol> sym = this;

  sym = sym->ResolveRef();

  while (sym->cls != sc_package)
    sym = sym->nameSpace;

  return sym;
}
