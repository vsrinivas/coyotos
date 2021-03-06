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
#include <stdio.h>
#include <dirent.h>
#include <errno.h>

#include <string>
#include <iostream>
#include <fstream>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include "SymTab.hxx"
#include "capidl.hxx"
#include "util.hxx"
#include "backend.hxx"

static void
symdump(GCPtr<Symbol> s, std::ostream& out, int indent)
{
  do_indent(out, indent);

  switch(s->cls){
  case sc_absinterface:
    // enabling stubs for abstract interface.  This can be double checked later
  case sc_interface:
    {
      if (s->isActiveUOC == false)
	return;

      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out, indent);

      break;
    }
  case sc_package:
  case sc_scope:
    {
      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out, indent);

      break;
    }
  case sc_operation:
    {
      GCPtr<Path> uocFileName = lookup_containing_file(s);

      {
	std::string sqn = s->QualifiedName('_');
	out << (target + Path(sqn + ".c"))
	    << ": " << *uocFileName << "\n";
	out << (execDir + archiveName) 
	    << "(" 
	    << (execDir + Path(sqn + ".o"))
	    << "): "
	    << (target + Path(sqn + ".c"))
	    << "\n";
	out << (execDir + archiveName)
	    << ": "
	    << (execDir + archiveName) 
	    << "(" 
	    << (execDir + Path(sqn + ".o"))
	    << ")\n";
      }

      break;
    }
  case sc_enum:
  case sc_bitset:
  case sc_struct:
  case sc_const:
  case sc_exception:
  case sc_typedef:

      break;

  default:
    {
      out << "UNKNOWN/BAD SYMBOL CLASS "
	  <<  s->ClassName()
	  << " FOR: "
	  << s->name
	  << "\n"; 
	     
      break;
    }
  }
}

void
output_c_stub_depend(GCPtr<Symbol> universalScope, BackEndFn fn)
{
  if (archiveName.isEmpty()) {
    fprintf(stderr, "Stub dependency generation requires an archive name\n");
    exit(1);
  }
  if (execDir.isEmpty()) {
    fprintf(stderr, "Stub dependency generation requires an executable directory\n");
    exit(1);
  }

  if (outputFileName.isEmpty() || outputFileName.asString() == "-") {
    symdump(universalScope, std::cout, 0);
  }
  else {
    std::ofstream out(outputFileName.c_str(),
		      std::ios_base::out|std::ios_base::trunc);
    if (!out.is_open()) {
      fprintf(stderr, "Couldn't open output file \"%s\" -- %s\n",
	      outputFileName.c_str(), strerror(errno));
      exit(1);
    }

    symdump(universalScope, out, 0);

    out.close();
  }
}
