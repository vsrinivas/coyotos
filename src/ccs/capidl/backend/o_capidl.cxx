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

#include <string>
#include <iostream>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include "SymTab.hxx"
#include "util.hxx"

// Forward declaration
void capidl_symdump(GCPtr<Symbol> s, std::ostream& out, int indent, bool withComments);

/* Dumping a type is mildly tricky, because in some cases it is done
   using the name and in other cases it is done using direct symbolic
   dump. 

   I'm actually not convinced that this is necessary, because all of
   these named types are invariably referenced using sc_symref, which
   seems to do the right thing entirely. */
void
capidl_dump_type(GCPtr<Symbol> s, std::ostream& out, int indent, bool withComments)
{
  switch(s->cls) {
  case sc_union:
  case sc_struct:
  case sc_enum:
  case sc_bitset:
  case sc_interface:
  case sc_absinterface:
  case sc_exception:
  case sc_typedef:
  case sc_symRef:
    {
      out << s->name;

      break;
    }

  case sc_seqType:
  case sc_arrayType:
  case sc_primtype:
  case sc_builtin:
    {
      capidl_symdump(s, out, indent, withComments);
      break;
    }

  default:
    {
      out << "UNKNOWN/BAD SYMBOL TYPE " << s->cls
	  << "FOR: "
	  << s->name;
      break;
    }
  }
}

void
capidl_symdump(GCPtr<Symbol>  s, std::ostream& out, int indent, bool withComments)
{
  if (withComments && s->docComment)
    PrintDocComment(s->docComment, out, indent+2);

  switch(s->cls){
    /* Deal with all of the scope-like elements: */
  case sc_package:
  case sc_scope:
  case sc_enum:
  case sc_bitset:
  case sc_struct:
  case sc_union:
  case sc_interface:
  case sc_absinterface:
    {
      unsigned i;

      if (s->cls == sc_absinterface)
	out << "abstract ";

      out << s->ClassName()
	  << " "
	  << s->name
	  << " ";

      if (s->baseType)
	out << "extends "
	    << s->baseType->QualifiedName('.')
	    << " ";

      if (s->raises.size() != 0) {
	unsigned i;
	out << "raises (";
	for(i = 0; i < s->raises.size(); i++) {
	  out << s->raises[i]->name;
	  if (i != (s->raises.size()-1))
	    out  << ", ";
	}

	out << ") ";

	fprintf(stderr, "CapIDL dump of interface raises "
		"is unimplemented \"%s\"\n",
		s->name.c_str());
	exit(1);
      }

      out << "{\n";

      for(i = 0; i < s->children.size(); i++) {
	capidl_symdump((s->children[i]), out, indent + 2, withComments);
	if (s->cls == sc_enum || s->cls == sc_bitset) {
	  if (i != (s->children.size()-1))
	    out << ",";
	  out << "\n";
	}
      }

      do_indent(out, indent);
      out << "};\n";

      break;
    }

  case sc_symRef:
    {
      out << s->name;

      break;
    }

  case sc_typedef:
    {
      if (withComments && s->docComment)
	PrintDocComment(s->docComment, out, indent+2);

      do_indent(out, indent);

      out << "typedef ";
      capidl_symdump(s->type, out, indent + 2, withComments);
      out << " " << s->name << ";";
      break;
    }

  case sc_builtin:
  case sc_exception:
  case sc_caseTag:
  case sc_caseScope:
  case sc_oneway:
  case sc_switch:
    out << "UNIMPLEMENTED SYMBOL TYPE "
	<< s->cls << " FOR: " << s->name;
      break;
#if 0
    {
      if (!symbol_IsScope(s) ||
	  (s->children.size() == 0 && !s->docComment && s->baseType == 0)) {
	fprintf(out, "<%s name=\"%s\" qname=\"%s\"/>\n", s->ClassName(), 
		     s->name, s->QualifiedName('_'));
      }
      else {
	unsigned i;

	fprintf(out, "<%s name=\"%s\" qname=\"%s\">\n", s->ClassName(), 
		     s->name, s->QualifiedName('_'));

	if (withComments && s->docComment)
	  PrintDocComment(s->docComment, out, indent+2);

	if (s->baseType) 
	  capidl_symdump(s->baseType, out, indent + 2, withComments);

	for(i = 0; i < s->children.size(); i++)
	  capidl_symdump((s->children[i]), out, indent + 2, withComments);

	do_indent(out, indent);
	fprintf(out, "</%s>\n", s->ClassName());
      }
      break;
    }
#endif
  case sc_primtype:
    {
      switch(s->v.lty){
      case lt_unsigned:
	out << "unsigned ";
	/* Fall through */
      case lt_integer:
	switch(s->v.bn.as_uint32()) {
	case 0:
	  out << "Integer";
	  break;
	case 8:
	  out << "byte";
	  break;
	case 16:
	  out << "short";
	  break;
	case 32:
	  out << "long";
	  break;
	case 64:
	  out << "long long";
	  break;
	default:
	  out << "/* unknown integer size */";
	  break;
	};

	break;
      case lt_char:
	switch(s->v.bn.as_uint32()) {
	case 0:
	case 32:
	  out << "wchar_t";
	  break;
	case 8:
	  out << "char";
	  break;
	default:
	  out << "/* unknown wchar size */";
	  break;
	}
	break;
      case lt_string:
	switch(s->v.bn.as_uint32()) {
	case 0:
	case 32:
	  out << "wstring";
	  break;
	case 8:
	  out << "string";
	  break;
	default:
	  out << "/* unknown string size */";
	  break;
	}
	break;
      case lt_float:
	switch(s->v.bn.as_uint32()) {
	case 32:
	  out << "float";
	  break;
	case 64:
	  out << "double";
	  break;
	case 128:
	  out << "long double";
	  break;
	default:
	  out << "/* unknown float size */";
	  break;
	}
	break;
      case lt_bool:
	{
	  out << "boolean";
	  break;
	}
      case lt_void:
	{
	  out << "void";
	  break;
	}
      default:
	out << "/* Bad primtype type code */";
      }
      break;
    }
  case sc_operation:
    {
      do_indent(out, indent);
      capidl_dump_type(s->type, out, indent, withComments);

      out << " " << s->name << "(";

      for(size_t i = 0; i < s->children.size(); i++) {
	capidl_symdump((s->children[i]), out, 0, withComments);
	if (i != s->children.size()-1)
	  out << ", ";
      }


      if (s->raises.size() > 0) {
	out << ")\n";
	do_indent(out, indent + 2);
	out << "raises (";
	for (size_t i = 0; i < s->raises.size(); i++) {
	  out << s->raises[i]->ResolveRef()->QualifiedName('.');
	  if (i != s->raises.size()-1)
	    out << ", ";
	}
      }

      out << ");\n";

      break;
    }

  case sc_arithop:
    {
      do_indent(out, indent);
      out << "(";

      if (s->children.size() > 1) {
	capidl_symdump(s->children[0], out, indent + 2, withComments);
	out << s->name;
	capidl_symdump(s->children[1], out, indent + 2, withComments);
      }
      else {
	out << s->name;
	capidl_symdump(s->children[0], out, indent + 2, withComments);
      }
      out << ")";

      break;
    }

  case sc_value:		/* computed constant values */
    {
      switch(s->v.lty){
      case lt_integer:
	{
	  std::string str = s->v.bn.asString(10);
	  out << str;
	  break;
	}
      case lt_unsigned:
	{
	  std::string str = s->v.bn.asString(10);
	  out << str << "u";
	  break;
	}
      case lt_float:
	{
	  out << s->v.d;
	  break;
	}
      case lt_char:
	{
	  std::string str = s->v.bn.asString(10);
	  out << "'" << str << "'";
	  break;
	}
      case lt_bool:
	{
	  out << (s->v.bn.as_uint32() ? "TRUE" : "FALSE");
	  break;
	}
      default:
	{
	  out << "/* lit UNKNOWN/BAD TYPE> */";
	  break;
	}
      };
      break;
    }

  case sc_const:		/* constant symbols, including enum values */
    {
      do_indent(out, indent);

      if ((s->nameSpace->cls != sc_enum) &&
	  (s->nameSpace->cls != sc_bitset)) {
	out << "const ";
	capidl_symdump(s->type, out, indent, withComments);
      }

      out << " " << s->name << " = ";
      capidl_symdump(s->value, out, 0, withComments);

      if ((s->nameSpace->cls != sc_enum) &&
	  (s->nameSpace->cls != sc_bitset))
	out << ";\n";
      break;
    }

  case sc_member:
    {
	do_indent(out, indent);

	capidl_symdump(s->type, out, indent+2, withComments);
	out << s->name << ";\n";

	break;
    }

  case sc_formal:
  case sc_outformal:
    {
      if (s->cls == sc_outformal)
	out << "out ";

      capidl_dump_type(s->type, out, 0, withComments);

      out << " " << s->name;
      break;
    }

  case sc_seqType:
    {
      out << "sequence<";
      capidl_symdump(s->type, out, 0, withComments);
      if (s->value) {
	out << ", *";
	capidl_symdump(s->value, out, 0, withComments);
      }
      out << ">";

      break;
    }

  case sc_arrayType:
    {
      capidl_symdump(s->type, out, 0, withComments);
      out << "[";
      capidl_symdump(s->value, out, 0, withComments);
      out << "]";

      break;
    }

  default:
    {
      out << "UNKNOWN/BAD SYMBOL TYPE " << s->cls
	  << " FOR: " << s->name;
      break;
    }
  }
}

void
output_capidl(GCPtr<Symbol> s)
{
  std::cout << "package " << s->nameSpace->QualifiedName('.')
	    << "\n\n";

  capidl_symdump(s, std::cout, 0, true);
}
