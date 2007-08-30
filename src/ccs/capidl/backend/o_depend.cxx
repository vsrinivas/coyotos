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
#include <dirent.h>

#include <string>
#include <iostream>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include "SymTab.hxx"

void
output_depend(GCPtr<Symbol> s)
{
  std::ostream& out = std::cout;

  out << "Dependencies for \""
      << s->QualifiedName('.')
      << "\"\n";

  SymVec vec;
  s->ComputeDependencies(vec);

  vec.qsort(Symbol::CompareByQualifiedName);

  for (size_t i = 0; i < vec.size(); i++)
    out << "  " 
	<< vec[i]->QualifiedName( '_')
	<< "\n";
}
