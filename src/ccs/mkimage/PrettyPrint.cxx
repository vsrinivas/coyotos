/*
 * Copyright (C) 2006, The EROS Group, LLC.
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
#include <stdint.h>
#include <dirent.h>
#include <string>
#include <iostream>
#include <sstream>
#include "AST.hxx"

using namespace sherpa;

void 
AST::PrettyPrint(INOstream& out) const
{
  size_t startIndent = out.indentToHere();

  out << '<' << astName();
  if (s.size())
    out << ' ' << '"' << s << '"';

  if (children->size()) {
    out << '>' << '\n';
    out.more();
    for (size_t c = 0; c < children->size(); c++) {
      child(c)->PrettyPrint(out);
      out << '\n';
    }
    out.less();
    out << '<' << '/' << astName() << '>';
  }
  else
    out << '/' << '>';

  out.setIndent(startIndent);
}

void 
AST::PrettyPrint(std::ostream& out) const
{
  INOstream ino_out(out);
  PrettyPrint(ino_out);
}
