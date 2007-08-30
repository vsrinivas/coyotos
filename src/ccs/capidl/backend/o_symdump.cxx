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
#include "util.hxx"

static void
symdump(GCPtr<Symbol> s, std::ostream& out, int indent)
{
  if (s->docComment)
    PrintDocComment(s->docComment, out, indent);

  do_indent(out, indent);

  switch(s->cls){
  case sc_scope:
  case sc_package:
  case sc_builtin:
  case sc_enum:
  case sc_bitset:
  case sc_struct:
  case sc_union:
  case sc_interface:
  case sc_absinterface:
  case sc_primtype:
  case sc_exception:
  case sc_caseTag:
  case sc_caseScope:
  case sc_oneway:
  case sc_typedef:
  case sc_switch:
  case sc_symRef:
  case sc_operation:
    {
      if (!s->IsScope() || (s->children.size() == 0 && !s->baseType)) {
	out << "<"
	    << s->ClassName()
	    << " name=\""
	    << s->name
	    << "\"/>\n";
      }
      else {
	out << "<"
	    << s->ClassName()
	    << " name=\""
	    << s->name
	    << "\">\n"; 

	if (s->baseType) {
	  do_indent(out, indent +2);
	  out << "<extends qname=\""
	      << s->QualifiedName('_')
	      << "\"/>\n";
	}

	for(size_t i = 0; i < s->children.size(); i++)
	  symdump(s->children[i], out, indent + 2);

	for(size_t i = 0; i < s->raises.size(); i++)
	  symdump(s->raises[i], out, indent + 2);

	do_indent(out, indent);
	out << "</"
	    << s->ClassName()
	    << ">\n";
      }
      break;
    }

  case sc_arithop:
    {
      out << "<"
	  << s->ClassName()
	  << " name=\""
	  << s->name
	  << "\">\n";		   

      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out,indent + 2);

      do_indent(out, indent);
      out << "</" << s->ClassName() << ">\n";

      break;
    }

  case sc_value:		/* computed constant values */
    {
      switch(s->v.lty){
      case lt_integer:
	{
	  std::string str = s->v.bn.asString(10);
	  out << "<lit int " << str << ">\n";
	  break;
	}
      case lt_unsigned:
	{
	  std::string str = s->v.bn.asString(10);
	  out << "<lit uint " << str << "\n";
	  break;
	}
      case lt_float:
	{
	  out << "<lit float " << s->v.d << ">\n";
	  break;
	}
      case lt_char:
	{
	  std::string str = s->v.bn.asString(10);
	  out << "<lit char \'" << str << "\'>\n";
	  break;
	}
      case lt_bool:
	{
	  out << "<lit bool " 
	      << (s->v.bn.as_uint32() ? "TRUE" : "FALSE") 
	      << ">\n";
	  break;
	}
      default:
	{
	  out << "<lit UNKNOWN/BAD TYPE>\n";
	  break;
	}
      };
      break;
    }

  case sc_const:		/* constant symbols, including enum values */
    {
      out << "<"
	  << s->ClassName()
	  << " name=\""
	  << s->name
	  << "\">\n";		   

      symdump(s->type, out, indent+2);
      symdump(s->value, out, indent+2);

      do_indent(out, indent);
      out << "</" << s->ClassName() << ">\n";
      break;
    }

  case sc_member:
  case sc_formal:
  case sc_outformal:
    {
      out << "<" 
	  << s->ClassName() 
	  << " name=\""
	  << s->name
	  << "\">\n";

      symdump(s->type, out,indent+2);

      do_indent(out, indent);
      out << "</" << s->ClassName() << ">\n";

      break;
    }
  case sc_seqType:
  case sc_arrayType:
    {
      out << "<" 
	  << s->ClassName() 
	  << " name=\""
	  << s->name
	  << "\">\n";

      symdump(s->type, out, indent+2);
      if (s->value)
	symdump(s->value, out, indent+2);

      do_indent(out, indent);
      out << "</" << s->ClassName() << ">\n";

      break;
    }

  default:
    {
      out << "UNKNOWN/BAD SYMBOL TYPE " 
	  << s->cls
	  << " ("
	  << s->ClassName()
	  << ") FOR: "
	  << s->name;
      break;
    }
  }
}

void
output_symdump(GCPtr<Symbol> s)
{
  symdump(s, std::cout, 0);
}
