/*
 * Copyright (C) 2004, The EROS Group, LLC.
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
#include <fstream>
#include <sstream>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include <libsherpa/Path.hxx>
#include "SymTab.hxx"
#include "util.hxx"
#include "backend.hxx"
#include "capidl.hxx"

static void
html_capidl_symdump(GCPtr<Symbol> s, std::ostream& out, int indent, bool withComments)
{
  extern void capidl_symdump(GCPtr<Symbol> s, std::ostream& out, int indent, bool withComments);

  std::ostringstream memout;

  capidl_symdump(s, memout, indent, withComments);

  const std::string& idl = memout.str();

  for (std::string::size_type i = 0; i < idl.size(); i++) {
    switch (idl[i]) {
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
      out << idl[i];
      break;
    }
  }
}

static void
output_BriefComment(GCPtr<Symbol> s, std::ostream& out, bool asPara)
{
  if (asPara)
    out << "<p>\n";

  if (!s->docComment)
    out << "(No synopsis)\n";
  else
    s->docComment->GetBriefDoc()->emitHtml(out);

  if (asPara)
    out << "</p>\n";
}

static void
output_DocComment(GCPtr<Symbol> s, std::ostream& out)
{
  if (!s->docComment) {
    out << "No documentation available.\n";
    return;
  }

  s->docComment->GetFullDoc()->emitHtml(out);

#if 0
  bool want_fill = false;

  do_indent(out, indent);
  out << "/**\n";

  do_indent(out, indent+2);
  
  while (*str) {
    if (want_fill) {
      do_indent(out, indent+2);
      want_fill = false;
    }

    if (*str == '\n')
      want_fill = true;
    out << *str++;
  }

  do_indent(out, indent);
  out << "*/\n";
#endif
}

static GCPtr<Symbol> 
symbol_getContainingFileSymbol(GCPtr<Symbol> s)
{
  s = s->ResolveRef();

  if (s == Symbol::UniversalScope)
    return s;

  if (s->cls == sc_package)
    return s;

  if (s->nameSpace && s->nameSpace->cls == sc_package)
    return s;

  return symbol_getContainingFileSymbol(s->nameSpace);
}

/// @brief Return the name of the HTML file in which this symbol should
/// appear.
Path
symbol_relative_filename(GCPtr<Symbol> current, GCPtr<Symbol> s)
{
  GCPtr<Symbol> fileSym = symbol_getContainingFileSymbol(s);
  GCPtr<Symbol> curFileSym = symbol_getContainingFileSymbol(current);
  std::string sqn = fileSym->QualifiedName('/');

  Path fileName;
  int depth = 0;

  // If in same file, no filename component:
  if (curFileSym == fileSym) return fileName;

  // Figure out relative path from current to root:
  while(current != Symbol::UniversalScope &&
	current->nameSpace != Symbol::UniversalScope) {
    depth++;
    current = current->nameSpace;
  }

  fileName = Path::CurDir;
  while(depth--)
    fileName = fileName + Path::ParentDir;

  fileName = fileName + Path(sqn);
  fileName = fileName << ".html";

  return fileName;
}

static std::string
symbol_linktarget(GCPtr<Symbol> current, GCPtr<Symbol> s)
{
  GCPtr<Symbol> fileSym = symbol_getContainingFileSymbol(s);

  /* Compute the local name part */
  if (s != fileSym) {
    std::string fileFQN = fileSym->QualifiedName('.');
    std::string fqn = s->QualifiedName('.');

    return fqn.substr(fileFQN.size() + 1);
  }

  return std::string();
}

static std::string
symbol_linkname(GCPtr<Symbol> current, GCPtr<Symbol> s)
{
  GCPtr<Symbol> fileSym = symbol_getContainingFileSymbol(s);

  const Path fileName = symbol_relative_filename(current, fileSym);

  /* Compute the local name part */
  if (s != fileSym) {
    std::string linkTarget = symbol_linktarget(current, s);

    return fileName.asString() + "#" + linkTarget;
  }

  return fileName.asString();
}

#if 0
static void
symdump(GCPtr<Symbol> s, std::ostream& out, int indent)
{
  do_indent(out, indent);

  switch(s->cls){
  case sc_package:
#if 0
    /* Don't output nested packages. */
    if (s->nameSpace->cls == sc_package)
      return;
#endif

  case sc_scope:
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
    {
      if (!symbol_IsScope(s) ||
	  (s->children.size() == 0 && s->docComment.empty() && s->baseType == 0)) {
	out << "&lt;"
	    << s->ClassName()
	    << " name =\""
	    << s->name.c_str()
	    << "\" qname = \""
	    << s->QualifiedName('_')
	    << "\"/>\n";
      }
      else {
	out << "&lt;"
	    << s->ClassName()
	    << " name =\""
	    << s->name.c_str()
	    << "\" qname = \""
	    << s->QualifiedName('_')
	    << "\"/>\n";

	if (!s->docComment.empty())
	  output_DocComment(s, out);

	if (s->baseType) {
	  do_indent(out, indent +2);
	  fprintf(out, "<extends qname=\"%s\"/>\n",
		       s->QualifiedName('_').c_str());
	}

	for(size_t i = 0; i < s->children.size(); i++)
	  symdump(s->children[i], out, indent + 2);

	for(size_t i = 0; i < s->raises.size(); i++)
	  symdump(s->raises[i], out, indent + 2);

	do_indent(out, indent);
	fprintf(out, "</%s>\n", s->ClassName().c_str());
      }
      break;
    }
  case sc_operation:
    {
      std::string type_name = s->type->name;

      bool use_nest = (s->children.size() != 0 || s->docComment.size());

      fprintf(out, "<%s name=\"%s\" qname=\"%s\" ty=\"%s\" tyclass=\"%s\"%s>\n", 
		   s->ClassName().c_str(), s->name.c_str(),
		   s->QualifiedName('_').c_str(),
		   type_name.c_str(), symbol_ClassName(s->type).c_str(),
		   use_nest ? "" : "/");

      if (use_nest) {
	if (!s->docComment.empty())
	  output_DocComment(s, out);

	for(size_t i = 0; i < s->children.size(); i++)
	  symdump(s->children[i], out, indent + 2);

	for(size_t i = 0; i < s->raises.size(); i++)
	  symdump(s->raises[i], out, indent + 2);

	do_indent(out, indent);
	fprintf(out, "</%s>\n", s->ClassName().c_str());
      }

      break;
    }

  case sc_arithop:
    {
      fprintf(out, "<%s name=\"%s\" qname=\"%s\">\n", 
	      s->ClassName().c_str(), 
	      s->name.c_str(), s->QualifiedName('_').c_str());

      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out, indent + 2);

      do_indent(out, indent);
      fprintf(out, "</%s>\n", s->ClassName().c_str());

      break;
    }

  case sc_value:		/* computed constant values */
    {
      switch(s->v.lty){
      case lt_integer:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  fprintf(out, "<lit int %s/>\n", str);
	  free(str);
	  break;
	}
      case lt_unsigned:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  fprintf(out, "<lit uint %s>\n", str);
	  free(str);
	  break;
	}
      case lt_float:
	{
	  fprintf(out, "<lit float %f/>\n", s->v.d);
	  break;
	}
      case lt_char:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  fprintf(out, "<lit char \'%s\'/>\n", str);
	  free(str);
	  break;
	}
      case lt_bool:
	{
	  fprintf(out, "<lit bool %s/>\n", mpz_get_ui(s->v.i) ? "TRUE" : "FALSE");
	  break;
	}
      default:
	{
	  fprintf(out, "<lit UNKNOWN/BAD TYPE>\n");
	  break;
	}
      };
      break;
    }

  case sc_const:		/* constant symbols, including enum values */
    {
      std::string type_name = s->type->name;

      fprintf(out, "<%s name=\"%s\" qname=\"%s\" ty=\"%s\" tyclass=\"%s\">\n", 
		   s->ClassName().c_str(), s->name.c_str(),
		   s->QualifiedName('_').c_str(),
		   type_name.c_str(), symbol_ClassName(s->type).c_str());

      if (!s->docComment.empty())
	output_DocComment(s, out);

      symdump(s->value, out, indent+2);

      do_indent(out, indent);
      fprintf(out, "</%s>\n", s->ClassName().c_str());

      break;
    }

  case sc_member:
  case sc_formal:
  case sc_outformal:
    {
      std::string type_name = s->type->name;

      if (!s->docComment.empty()) {
	fprintf(out, "<%s name=\"%s\" qname=\"%s\" ty=\"%s\" tyclass=\"%s\">\n", 
		s->ClassName().c_str(), s->name.c_str(),
		s->QualifiedName('_').c_str(),
		type_name.c_str(), symbol_ClassName(s->type).c_str());

	output_DocComment(s, out);

	do_indent(out, indent);
	fprintf(out, "</%s>\n", s->ClassName().c_str());
      }
      else {
	fprintf(out, "<%s name=\"%s\" qname=\"%s\" ty=\"%s\" tyclass=\"%s\"/>\n", 
		s->ClassName().c_str(), s->name.c_str(),
		s->QualifiedName('_').c_str(),
		type_name.c_str(), symbol_ClassName(s->type).c_str());
      }

      break;
    }

  case sc_seqType:
  case sc_arrayType:
    {
      fprintf(out, "<%s name=\"%s\">\n", s->ClassName().c_str(),
	      s->name.c_str());

      symdump(s->type, out, indent+2);
      if (s->value)
	symdump(s->value, out, indent+2);

      do_indent(out, indent);
      fprintf(out, "</%s>\n", s->ClassName().c_str());

      break;
    }

  default:
    {
      fprintf(out, "UNKNOWN/BAD SYMBOL TYPE %d FOR: %s", 
		   s->cls, s->name.c_str());
      break;
    }
  }
}
#endif

static void
emit_html_header(std::ostream& out, const std::string& title, const std::string& category)
{
  out << "<html>\n";
  out << "<head>\n";
  out << "<title>";
  if (title.size()) out << title;
  if (title.size() && category.size()) out << " ";
  if (category.size()) out << "(" << category << ")";
  out << "</title>\n";
  out << "<style type='text/css'>\n";
  out << "<!--\n";
  out << "a { text-decoration: none }\n";
  out << "pre.capidl { background-color: lightgrey; }\n";
  out << "-->\n";
  out << "</style>\n";
  out << "</head>\n";
  out << "<body>\n";
  out << "<center>\n";
  if (title.size()) out << "<h1><tt>" << title << "</tt></h1>";
  if (category.size()) out << "<p>(" << category << ")</p>";
  out << "</center>\n";
}

static void
emit_html_trailer(std::ostream& out, const std::string& category, 
		  const std::string& symname)
{
  out << "</body>\n";
  out << "</html>\n";

}

static void
output_symbol_index(GCPtr<Symbol> current, GCPtr<Symbol> s, std::ostream& out, int indent)
{
  switch(s->cls) {
  case sc_enum:
  case sc_bitset:
  case sc_struct:
  case sc_union:
  case sc_absinterface:
  case sc_interface:
  case sc_typedef:
  case sc_package:
  case sc_operation:
  case sc_oneway:
  case sc_exception:
    {
      out << "<tr valign='top'>"
	  << "<td><a href=\""
	  << symbol_linkname(current, s)
	  << "\">"
	  << s->QualifiedName('.')
	  << "</a></td>"
	  << "<td>"
	  << s->ClassPrintName()
	  << "</td>"
	  << "<td>";
      output_BriefComment(s, out, false);
      out << "</td>"
	  <<"</tr>\n";
      break;
    }
  default:
    break;
  }

  {
    SymVec vec = s->children;

    vec.qsort(Symbol::CompareByName);
    
    /* Export subordinate packages first! */
    for (size_t i = 0; i < vec.size(); i++) {
      GCPtr<Symbol> child = vec[i];

      output_symbol_index(current, child, out, indent);
    }
  }
}

static void
output_byname(GCPtr<Symbol> globalScope)
{
  Path fileName = target + Path("byname.html");
  std::ofstream out(fileName.c_str(), std::ios_base::out|std::ios_base::trunc);
  if (!out.is_open()) {
    fprintf(stderr, "Couldn't open output file \"%s\"\n",
	    fileName.c_str());
    exit(1);
  }

  emit_html_header(out, "Index of Symbols", std::string());

  out << "<table>\n";

  output_symbol_index(globalScope, globalScope, out, 0);

  out << "</table>\n";

  emit_html_trailer(out, "Index of Symbols", std::string());

  out.close();
}

static void
output_this_package_page(GCPtr<Symbol> s)
{
  Path fileName = symbol_relative_filename(Symbol::UniversalScope, s);
  Path dirName = fileName.dirName();

  std::cout << "Emitting package page "
	    << fileName <<" in directory " << dirName << "\n";

  dirName.smkdir();

  std::ofstream out(fileName.c_str(), std::ios_base::out|std::ios_base::trunc);
  if (!out.is_open()) {
    fprintf(stderr, "Couldn't open output file \"%s\"\n",
	    fileName.c_str());
    exit(1);
  }

  emit_html_header(out, s->QualifiedName('.'), "package");

  output_BriefComment(s, out, true);

  out << "<p>\n";
  if (s->nameSpace && s->nameSpace != Symbol::UniversalScope) {
    out << "Parent: <a href=\""
	<< symbol_relative_filename(s, s->nameSpace)
	<< "\">"
	<< s->nameSpace->QualifiedName('.')
	<< "</a>\n";
  }
  out << "</p>\n";

  out << "<h1>Description</h1>\n";
  output_DocComment(s, out);

  out << "<h1>Index of Symbols</h1>\n";
  out << "<table>\n";

  {
    SymVec vec = s->children;
    vec.qsort(Symbol::CompareByName);

    /* Export subordinate packages first! */
    for (size_t i = 0; i < vec.size(); i++) {
      GCPtr<Symbol> child = vec[i];

      output_symbol_index(s, child, out, 0);
    }
  }

  out << "</table>\n";

  emit_html_trailer(out, "Package", s->QualifiedName('.'));

  out.close();
}

static void
output_package_pages(GCPtr<Symbol> s)
{
  if (s->cls == sc_package)
    output_this_package_page(s);

  {
    SymVec vec = s->children;
    vec.qsort(Symbol::CompareByName);

    /* Export subordinate packages first! */
    for (size_t i = 0; i < vec.size(); i++) {
      GCPtr<Symbol> child = vec[i];

      output_package_pages(child);
    }
  }
}

static void
output_this_symbol_page(GCPtr<Symbol> s)
{
  Path fileName = symbol_relative_filename(Symbol::UniversalScope, s);
  Path dirName = fileName.dirName();

  /* children sorted alphabetically */
  SymVec acvec = s->children;
  acvec.qsort(Symbol::CompareByName);

  fprintf(stdout, "Emitting %s page %s in directory %s\n", 
	  s->ClassPrintName().c_str(),
	  fileName.c_str(), dirName.c_str());

  dirName.smkdir();

  std::ofstream out(fileName.c_str(), std::ios_base::out|std::ios_base::trunc);
  if (!out.is_open()) {
    fprintf(stderr, "Couldn't open output file \"%s\"\n",
	    fileName.c_str());
    exit(1);
  }

  emit_html_header(out, s->QualifiedName('.'), s->ClassPrintName());

  output_BriefComment(s, out, true);

  assert (s->nameSpace && s->nameSpace != Symbol::UniversalScope);

  out << "<p>\n";
  out << "Package: <a href=\""
      << symbol_relative_filename(s, s->ResolvePackage())
      << "\">"
      << s->ResolvePackage()->QualifiedName('.')
      << "</a>\n";
  out << "</p>\n";

  out << "<p>\n";
  out << "Parent: <a href=\""
      << symbol_relative_filename(s, s->nameSpace)
      << "\">"
      << s->nameSpace->QualifiedName('.')
      << "</a>\n";
  out << "</p>\n";

  if (s->baseType) {
    GCPtr<Symbol> baseType = s->baseType->ResolveRef();

    out << "<p>\n";
    out << "Inherits from <a href=\""
	<< symbol_relative_filename(s, baseType)
	<< "\">"
	<< baseType->QualifiedName('.')
	<< "</a>.\n",
    out << "</p>\n";
  }

  out << "<h1>Description</h1>\n";
  output_DocComment(s, out);

  // Wrap the whole thing in a single table so that all of the symbol names
  // and descriptions align properly:

  out << "<table>\n";

  if ( s->CountChildren(&Symbol::IsConstantSymbol) ) {
    out << "<tr valign='top'>\n";
    out << "<td colspan='2'>\n";
    out << "<h1>Constants and Enumerations</h1>";
    out << "</td>\n";
    out << "</tr>\n";

    for (size_t i = 0; i < acvec.size(); i++) {
      GCPtr<Symbol> child = acvec[i];

      if (child->IsConstantSymbol()) {
	out << "<tr valign='top'>\n";
	out << "<td><a name=\""
	    << symbol_linktarget(s, child)
	    << "\">"
	    << child->name
	    << "</a></td>\n";

	out << "<td>";
	output_BriefComment(child, out, true);

	out << "<p>\n";
	out << "<ul>\n";
	out << "<pre class='capidl'>\n";
	html_capidl_symdump(child, out, 0, false);
	out << "</pre>";
	out << "</ul>";
	out << "</p>\n";

	output_DocComment(child, out);

	out << "</td>";
	out << "</tr>";
      }
    }

    if ( s->CountChildren(&Symbol::IsEnumSymbol) ) {
      const char *commasep = "";

      out << "<tr valign='top'>\n";
      out << "<td colspan='2'>\n";
      out << "<p>\n";
      out << "See also: ";

      for (size_t i = 0; i < acvec.size(); i++) {
	GCPtr<Symbol> child = acvec[i];

	if (child->IsEnumSymbol()) {
	  out << commasep
	      << "<a href=\"#"
	      << symbol_linktarget(s, child)
	      << "\">"
	      << child->name
	      << "</a>\n";
		 
	  commasep = ", ";
	}
      }

      out << "</p>\n";
      out << "</td>";
      out << "</tr>";
    }
  }

  if ( s->CountChildren(&Symbol::IsTypeSymbol) ) {
    out << "<tr valign='top'>\n";
    out << "<td colspan='2'>\n";
    out << "<h1>Types</h1>";
    out << "</td>\n";
    out << "</tr>\n";

    for (size_t i = 0; i < acvec.size(); i++) {
      GCPtr<Symbol> child = acvec[i];

      if (child->IsTypeSymbol()) {
	out << "<tr valign='top'>\n";
	out << "<td><a name=\""
	    << symbol_linktarget(s, child)
	    << "\">"
	    <<child->name
	    << "</a></td>\n";

	out << "<td>";
	output_BriefComment(child, out, true);

	out << "<p>\n";
	out << "<ul>\n";
	out << "<pre class='capidl'>";
	html_capidl_symdump(child, out, 0, false);
	out << "</pre>";
	out << "</ul>";
	out << "</p>\n";

	output_DocComment(child, out);

	out << "</td>";
	out << "</tr>";
      }
    }
  }

  if ( s->CountChildren(&Symbol::IsOperation) ) {
    out << "<tr valign='top'>\n";
    out << "<td colspan='2'>\n";
    out << "<h1>Methods</h1>";
    out << "</td>\n";
    out << "</tr>\n";

    for (size_t i = 0; i < acvec.size(); i++) {
      GCPtr<Symbol> child = acvec[i];

      if (child->IsOperation()) {
	out << "<tr valign='top'>\n";
	out << "<td><a name=\""
	    << symbol_linktarget(s, child)
	    << "\">"
	    << child->name
	    << "</a></td>\n";

	out << "<td>";
	output_BriefComment(child, out, true);

	out << "<p>\n";
	out << "<ul>\n";
	out << "<pre class='capidl'>";
	html_capidl_symdump(child, out, 0, false);
	out << "</pre>";
	out << "</ul>";
	out << "</p>\n";

	output_DocComment(child, out);

	out << "</td>";
	out << "</tr>";
      }
    }

  }

  out << "</table>\n";

  if (acvec.size() > 0) {
    out << "<h1>Index of Symbols</h1>\n";
    out << "<table>\n";

    {
      /* Export subordinate packages first! */
      for (size_t i = 0; i < acvec.size(); i++) {
	GCPtr<Symbol> child = acvec[i];

	output_symbol_index(s, child, out, 0);
      }
    }

    out << "</table>\n";
  }

  emit_html_trailer(out, "Interface", s->QualifiedName('.'));

  out.close();
}

static void
output_symbol_pages(GCPtr<Symbol> s)
{
  if (s->nameSpace && s->nameSpace->cls == sc_package)
    output_this_symbol_page(s);

  {
    SymVec vec = s->children;
    vec.qsort(Symbol::CompareByName);

    /* Export subordinate packages first! */
    for (size_t i = 0; i < vec.size(); i++) {
      GCPtr<Symbol> child = vec[i];

      output_symbol_pages(child);
    }
  }
}

void
output_html(GCPtr<Symbol> globalScope, BackEndFn fn)
{
  target.smkdir();

  if (opt_index) {
    output_byname(globalScope);
    output_package_pages(globalScope);
  }

  output_symbol_pages(globalScope);
  //  symdump(globalScope, stdout, 0);
}
