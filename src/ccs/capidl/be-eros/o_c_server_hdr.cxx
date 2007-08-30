/*
 * Copyright (C) 2003, The EROS Group, LLC.
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
#include <stdio.h>
#include <errno.h>
#include <dirent.h>

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include "SymTab.hxx"
#include "util.hxx"
#include "backend.hxx"
#include "capidl.hxx"

extern void output_c_type(GCPtr<Symbol> s, std::ostream& out, int indent);

static void
calc_sym_depend(GCPtr<Symbol> s, SymVec& vec)
{
  switch(s->cls) {
  case sc_absinterface:
  case sc_interface:
    {
      s->ComputeDependencies(vec);

      {
	GCPtr<Symbol> targetUoc = s->UnitOfCompilation();
	if (!vec.contains(targetUoc))
	  vec.append(targetUoc);
      }

      for(size_t i = 0; i < s->children.size(); i++)
	calc_sym_depend(s->children[i], vec);
    }

  case sc_operation:
    {
      s->ComputeDependencies(vec);
      return;
    }

  default:
    return;
  }

  return;
}

static void
compute_server_dependencies(GCPtr<Symbol> scope, SymVec& vec)
{
  /* Export subordinate packages first! */
  for (size_t i = 0; i < scope->children.size(); i++) {
    GCPtr<Symbol> child = scope->children[i];

    if (child->cls != sc_package && child->isActiveUOC)
      calc_sym_depend(child, vec);

    if (child->cls == sc_package)
      compute_server_dependencies(child, vec);
  }

  return;
}

static void
emit_op_dispatcher(GCPtr<Symbol> s, std::ostream& out)
{
  out <<"\nfixreg_t implement_"
      << s->QualifiedName('_')
      << "(";
  
  for (size_t i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> arg = s->children[i];
    GCPtr<Symbol> argType = arg->type->ResolveRef();
    GCPtr<Symbol> argBaseType = argType->ResolveType();
    
    if (i > 0)
      out << ", ";
    
    if (arg->cls == sc_formal) {
      output_c_type(argBaseType, out, 0);
      out << " " << arg->name;
    } else {
      output_c_type(argBaseType, out, 0);
      out << " * " << arg->name << " /* OUT */";
    }
  }
  out << ");\n";

}

static void
emit_decoders(GCPtr<Symbol> s, std::ostream& out)
{
  if (s->mark)
    return;

  s->mark = true;

  switch(s->cls) {
  case sc_absinterface:
  case sc_interface:
    {
      if (s->baseType)
	emit_decoders(s->baseType->ResolveRef(), out);

      for(size_t i = 0; i < s->children.size(); i++)
	emit_decoders(s->children[i], out);

      return;
    }

  case sc_operation:
    {
      if (s->isActiveUOC)
	emit_op_dispatcher(s, out);

      return;
    }

  default:
    return;
  }

  return;
}

void
emit_server_header_decoders(GCPtr<Symbol> scope, std::ostream& out)
{
  unsigned i;

  /* Export subordinate packages first! */
  for (i = 0; i < scope->children.size(); i++) {
    GCPtr<Symbol> child = scope->children[i];

    if (child->cls != sc_package && child->isActiveUOC)
      emit_decoders(child, out);

    if (child->cls == sc_package)
      emit_server_header_decoders(child, out);
  }

  return;
}

void 
output_c_server_hdr (GCPtr<Symbol> universalScope, BackEndFn fn)
{
  if (outputFileName.isEmpty()) {
    fprintf(stderr, "Need to provide output file name.\n");
    exit(1);
  }

  SymVec vec;
  compute_server_dependencies(universalScope, vec);

  std::stringstream preamble;

  preamble << "#if !defined(__STDC_VERSION__) ||"
	   << " (__STDC_VERSION__ < 19901L)\n";
  preamble << "#error \"bitcc output requires C99 support\"\n";
  preamble << "#endif\n";

  preamble << "#include <stdbool.h>\n";
  preamble << "#include <stddef.h>\n";
  preamble << "#include <eros/target.h>\n";

  for (size_t i = 0; i < vec.size(); i++) {
    preamble << "#include <idl/";
    preamble << vec[i]->QualifiedName( '/');
    preamble << ".h>\n";
  }

  universalScope->ClearAllMarks();

  if (outputFileName.asString() == "-") {
    std::ostream& out = std::cout;

    out << preamble.str();

    emit_server_header_decoders(universalScope, out);
  }
  else {
    std::ofstream out(outputFileName.c_str(), 
		      std::ios_base::out|std::ios_base::trunc);

    if (!out.is_open()) {
      fprintf(stderr, "Couldn't open stub source file \"%s\"\n",
	      outputFileName.c_str());
      exit(1);
    }

    out << preamble.str();

    emit_server_header_decoders(universalScope, out);

    out.close();
  }
}
