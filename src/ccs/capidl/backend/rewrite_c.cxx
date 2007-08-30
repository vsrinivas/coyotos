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
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>

#include <string>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include "SymTab.hxx"
#include "util.hxx"

#define REGISTER_BITS    32
#define ENUMERAL_SIZE    4
#define TARGET_LONG_SIZE 4

static void
fixup(GCPtr<Symbol> s)
{
  unsigned i;

  switch(s->cls){
  case sc_operation:
    {
      assert(Symbol::VoidType->IsVoidType());

      /* If the return type is non-void, stick it on the end of the
       * parameter list as an OUT parameter named "_retVal" */
      if (s->type->IsVoidType() == false) {
	GCPtr<Symbol> retVal = 
	  Symbol::Create_inScope(LToken(s->loc, "_retVal"), s->isActiveUOC, 
				 sc_outformal, s);
	retVal->type = s->type;
	s->type = Symbol::VoidType;
      }

#if 0
      /* Reorder the parameters so that all of the IN parameters
       * appear first. */
      {
	unsigned int first_out = UINT_MAX;

	for (i = 0; i < s->children.size(); i++) {
	  GCPtr<Symbol> child = s->children[i];

	  if (child->cls == sc_outformal && i < first_out) {
	    first_out = i;
	    continue;
	  }

	  if (child->cls == sc_formal && i > first_out) {
	    unsigned j;
	    for (j = i; j > first_out; j--)
	      s->children[j] = s->children[j-1];
	    s->children[first_out] = child;
	    first_out++;
	  }
	}
      }
#endif

      break;
    }
  default:
    {
      /* Values do not need to be rewritten for C (ever) */
      /* Types need to be rewritten, but we rely on the fact that the
       * xformer is run over the entire input tree, and the type
       * symbol will therefore be caught as the child of some scope. */
      for(i = 0; i < s->children.size();i++)
	fixup(s->children[i]);

      break;
    }
  }
}

bool
c_typecheck(GCPtr<Symbol> s)
{
  unsigned i;
  bool result = true;
  GCPtr<Symbol> ty = s->type;

  if (ty) {
    static std::string pure_int = "#int";
    static std::string pure_ws8 = "#wstring8";
    static std::string pure_ws32 = "#wstring32";

    if (ty->name == pure_int ||
	ty->name == pure_ws8 ||
	ty->name == pure_ws32) {
      fprintf(stderr, "%s \"%s\" specifies unbounded type  \"%s\"\n",
		   s->ClassName().c_str(),
		   s->QualifiedName('.').c_str(),
		   ty->name.c_str());
      result = false;
    }

    if (ty->cls == sc_seqType && !ty->value) {
      fprintf(stderr, "%s \"%s\" specifies unbounded sequence type, which is not (yet) supported\n",
		  s->ClassName().c_str(),
		  s->QualifiedName('.').c_str());
      result = false;
    }

    if (ty->cls == sc_bufType && !ty->value) {
      fprintf(stderr, "%s \"%s\" specifies unbounded buffer type, which is not (yet) supported\n",
		  s->ClassName().c_str(),
		  s->QualifiedName('.').c_str());
      result = false;
    }
  }

  if (s->cls == sc_exception && s->children.size() != 0) {
    fprintf(stderr, "%s \"%s\" EROS exceptions should not have members\n",
		s->ClassName().c_str(),
		s->QualifiedName('.').c_str());
    result = false;
  }

  if (s->cls == sc_symRef)
    return result;

  for(i = 0; i < s->children.size(); i++)
    result = result && c_typecheck(s->children[i]);

  return result;
}

void
rewrite_for_c(GCPtr<Symbol> s)
{
  fixup(s);
}
