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
#include "capidl.hxx"
#include "util.hxx"
#include "INOstream.hxx"

static
std::string
CleanTypeName(std::string nm)
{
  if (nm[0] == '#')
    return nm.substr(1);
  return nm;
}

static void
xmlPrintDocComment(const DocComment *dc, INOstream& out)
{
  if (!dc)
    return;

#if 0
  bool want_fill = false;
#endif
  std::string docString = dc->asString();

  out << "<capdoc:description>\n";

  out.more();
  out << "<capdoc:complete>";
  
  for (size_t i = 0; i < docString.length(); i++) {
    char c = docString[i];
  // Do not indent the content, as it may contain preformatted text
  // in which white space is significant!

    switch (c) {
    case '&':
      out << "&amp;";
      break;
    case '<':
      out << "&lt;";
      break;
    case '>':
      out << "&gt;";
      break;
    default:
      out << c;
      break;
    }
  }

  // Similarly, do not indent the closing tag for the complete comment,
  // because that would add white space to the comment body.
  out << "</capdoc:complete>\n";

  if (dc->HasBriefDoc()) {
    out << "<capdoc:brief>";
    dc->GetBriefDoc()->emitHtml(out.ostrm);
    out << "</capdoc:brief>\n";
  }

  if (dc->HasFullDoc()) {
    out << "<capdoc:full>";
    dc->GetFullDoc()->emitHtml(out.ostrm);
    out << "</capdoc:full>\n";
  }

  out.less();
  out << "</capdoc:description>\n";
}

static void
symdump(GCPtr<Symbol> s, INOstream& out)
{
  switch(s->cls){
  case sc_package:
#if 0
    /* Don't output nested packages. */
    if (s->nameSpace->cls == sc_package)
      return;
#endif

  case sc_symRef:
  case sc_primtype:
    {
      out << "<capdoc:"
	  << s->ClassName()
	  << " name=\""
	  << s->name
	  << "\" qname=\""
	  << s->QualifiedName('.')
	  << "\"/>\n";
      break;
    }

  case sc_scope:
  case sc_builtin:
  case sc_enum:
  case sc_bitset:
  case sc_struct:
  case sc_union:
  case sc_interface:
  case sc_absinterface:
  case sc_exception:
  case sc_caseTag:
  case sc_caseScope:
  case sc_oneway:
  case sc_inhOneway:
  case sc_typedef:
  case sc_switch:
    {
      out << "<capdoc:"
	  << s->ClassName()
	  << " name=\""
	  << s->name
	  << "\" qname=\""
	  << s->QualifiedName('.')
	  << "\">\n";

      out.more();

      xmlPrintDocComment(s->docComment, out);

      if (s->baseType) {
	GCPtr<Symbol> baseType = s->baseType;

	while (baseType) {
	  baseType = baseType->ResolveRef();

	  out << "<capdoc:extends name=\""
	      << baseType->name
	      << "\" qname=\""
	      << baseType->QualifiedName('.')
	      << "\"/>\n";
	  
	  baseType = baseType->baseType;
	}
      }

      if (s->type) {
	out << "<capdoc:type name=\""
	    << s->type->name
	    << "\" qname=\""
	    << s->type->QualifiedName('.')
	    << "\" ty=\""
	    << CleanTypeName(s->type->name)
	    << "\" tyclass=\""
	    << s->type->ClassName()
	    << "\">\n";
	out.more();
	symdump(s->type, out);
	out.less();
	out << "</capdoc:type>\n";
      }

      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out);

      if (s->raises.size()) {
	out << "<capdoc:raises>\n";
	out.more();

	for(size_t i = 0; i < s->raises.size(); i++)
	  symdump(s->raises[i], out);

	out.less();
	out << "</capdoc:raises>\n";
      }

      out.less();
      out << "</capdoc:" << s->ClassName() << ">\n";
      break;
    }
  case sc_operation:
  case sc_inhOperation:
    {
      out << "<capdoc:"
	  << s->ClassName()
	  << " name=\""
	  << s->name
	  << "\" qname=\""
	  << s->QualifiedName('.')
	  << "\" ty=\""
	  << CleanTypeName(s->type->name)
	  << "\" tyclass=\""
	  << s->type->ClassName()
	  << "\""
	  << ">\n";
      out.more();

      if (s->type) {
	out << "<capdoc:type name=\""
	    << s->type->name
	    << "\" qname=\""
	    << s->type->QualifiedName('.')
	    << "\" ty=\""
	    << CleanTypeName(s->type->name)
	    << "\" tyclass=\""
	    << s->type->ClassName()
	    << "\">\n";
	out.more();
	symdump(s->type, out);
	out.less();
	out << "</capdoc:type>\n";
      }

      xmlPrintDocComment(s->docComment, out);

      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out);

      if (s->raises.size()) {
	out << "<capdoc:raises>\n";
	out.more();
	for(size_t i = 0; i < s->raises.size(); i++)
	  symdump(s->raises[i], out);
	out.less();
	out << "</capdoc:raises>\n";
      }

      out.less();
      out << "</capdoc:" << s->ClassName() << ">\n";

      break;
    }

  case sc_arithop:
    {
      out << "<capdoc:"
	    << s->ClassName()
	    << " name=\""
	    << s->name
	    << "\" qname=\""
	    << s->QualifiedName('.')
	    << "\">\n";

      out.more();

      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out);

      out.less();
      out << "</capdoc:" << s->ClassName() << ">\n";

      break;
    }

  case sc_value:		/* computed constant values */
    {
      switch(s->v.lty){
      case lt_integer:
	{
	  std::string str = s->v.bn.asString(10);
	  out << "<capdoc:literal ty=\"int\" value=\"" << str << "\"/>\n";
	  break;
	}
      case lt_unsigned:
	{
	  std::string str = s->v.bn.asString(10);
	  out << "<capdoc:literal ty=\"uint\" value=\"" << str << "\"/>\n";
	  break;
	}
      case lt_float:
	{
	  out << "<capdoc:literal ty=\"float\" value=\"" << s->v.d << "\"/>\n";
	  break;
	}
      case lt_char:
	{
	  std::string str = s->v.bn.asString(10);
	  out << "<capdoc:literal ty=\"char\" value=\'" << str << "\'/>\n";
	  break;
	}
      case lt_bool:
	{
	  out << "<capdoc:literal ty=\"bool\" value=\"" 
	      << (s->v.bn.as_uint32() ? "true" : "false") 
	      << "\"/>\n";
	  break;
	}
      default:
	{
	  out << "<capdoc:lit UNKNOWN/BAD TYPE>\n";
	  break;
	}
      };
      break;
    }

  case sc_const:		/* constant symbols, including enum values */
    {
      out << "<capdoc:"
	  << s->ClassName()
	  << " name=\""
	  << s->name
	  << "\" qname=\""
	  << s->QualifiedName('.')
	  << "\" ty=\""
	  << CleanTypeName(s->type->name)
	  << "\" tyclass=\""
	  << s->type->ClassName()
	  << "\">\n";
      out.more();

      if (s->type) {
	out << "<capdoc:type name=\""
	    << s->type->name
	    << "\" qname=\""
	    << s->type->QualifiedName('.')
	    << "\" ty=\""
	    << CleanTypeName(s->type->name)
	    << "\" tyclass=\""
	    << s->type->ClassName()
	    << "\">\n";
	out.more();
	symdump(s->type, out);
	out.less();
	out << "</capdoc:type>\n";
      }

      xmlPrintDocComment(s->docComment, out);

      symdump(s->value, out);

      out.less();
      out << "</capdoc:" << s->ClassName() << ">\n";

      break;
    }

  case sc_member:
  case sc_formal:
  case sc_outformal:
    {
      out << "<capdoc:"
	  << s->ClassName()
	  << " name=\""
	  << s->name
	  << "\" qname=\""
	  << s->QualifiedName('.')
	  << "\" ty=\""
	  << CleanTypeName(s->type->name)
	  << "\" tyclass=\""
	  << s->type->ClassName()
	  << "\">\n";
      out.more();

      if (s->type) {
	out << "<capdoc:type name=\""
	    << s->type->name
	    << "\" qname=\""
	    << s->type->QualifiedName('.')
	    << "\" ty=\""
	    << CleanTypeName(s->type->name)
	    << "\" tyclass=\""
	    << s->type->ClassName()
	    << "\">\n";
	out.more();
	symdump(s->type, out);
	out.less();
	out << "</capdoc:type>\n";
      }

      xmlPrintDocComment(s->docComment, out);

      out.less();
      out << "</capdoc:" << s->ClassName() << ">\n";

      break;
    }

  case sc_seqType:
  case sc_arrayType:
    {
      out << "<capdoc:"
	  << s->ClassName()
	  << " name=\""
	  << s->name
	  << "\">\n";
      out.more();

      if (s->type) {
	out << "<capdoc:type name=\""
	    << s->type->name
	    << "\" qname=\""
	    << s->type->QualifiedName('.')
	    << "\" ty=\""
	    << CleanTypeName(s->type->name)
	    << "\" tyclass=\""
	    << s->type->ClassName()
	    << "\">\n";
	out.more();
	symdump(s->type, out);
	out.less();
	out << "</capdoc:type>\n";
      }

      if (s->value)
	symdump(s->value, out);

      out.less();
      out << "</capdoc:" << s->ClassName() << ">\n";

      break;
    }

  default:
    {
      out << "UNKNOWN/BAD SYMBOL TYPE "
	  << s->cls
	  << " FOR: "
	  << s->name;
      break;
    }
  }
}

void
output_xmldoc(GCPtr<Symbol> s)
{
  if (s->isActiveUOC == false)
    return;

  INOstream out(std::cout);

  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

// out << "<!DOCTYPE capdoc:obdoc SYSTEM \"../../../DTD/doc.dtd\">\n";
  // out << "<obdoc xmlns:capdoc=''>\n";
  out << "<obdoc xmlns:capdoc='file:/dev/null' "
      << "xmlns:doxygen='file:/dev/null'>\n";

  out.more();

  symdump(s, out);

  out.less();

  out << "</obdoc>\n";
}
