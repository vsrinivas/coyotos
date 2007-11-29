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

#include <sys/types.h>
#include <assert.h>
#include <dirent.h>

#include <iostream>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include "DomNode.hxx"
#include "util.hxx"

DomNode::~DomNode()
{
}

RootDomNode::~RootDomNode()
{
}

TextDomNode::~TextDomNode()
{
}

WhiteSpaceDomNode::~WhiteSpaceDomNode()
{
}

ElemDomNode::~ElemDomNode()
{
}

AttrDomNode::~AttrDomNode()
{
}

void
DomNode::dump(std::ostream& out, int indent) const
{
  out << indent;

  do_indent(out, indent);

  switch(nodeType) {
  case DomNode::ntNone:
    {
      out << "<no type>\n";

      break;
    }
  case DomNode::ntRoot:
    {
      out << "Root: " << name << "\n";

      break;
    }
  case DomNode::ntElem:
    {
      out << "Elem: " << name << "\n";

      break;
    }
  case DomNode::ntAttr: {
    {
      const AttrDomNode *an = 
	dynamic_cast<const AttrDomNode *> (this);
      assert(an);
      out << "Attr: " << name << " = " << an->value << "\n";

      break;
    }
  }
  case DomNode::ntText:
    {
      const TextDomNode *tn = 
	dynamic_cast<const TextDomNode *> (this);
      assert(tn);
      out << "Text: " << tn->text << "\n";

      break;
    }
  case DomNode::ntWhiteSpace:
    {
      const WhiteSpaceDomNode *wsn = 
	dynamic_cast<const WhiteSpaceDomNode *> (this);
      assert(wsn);
      out << "WS: " << wsn->text << "\n";

      break;
    }
  }

  for (size_t i = 0; i < attrs.size(); i++)
    attrs[i]->dump(out, indent+2);

  for (size_t i = 0; i < children.size(); i++)
    children[i]->dump(out, indent+2);
}

void
DomNode::emitHtml(std::ostream& out) const
{
  switch(nodeType) {
  case DomNode::ntNone:
    {
      fprintf(stderr, "Internal error in DOM tree construction\n");
      exit(1);
    }
  case DomNode::ntRoot:
    {
      for (size_t i = 0; i < attrs.size(); i++)
	attrs[i]->emitHtml(out);

      for (size_t i = 0; i < children.size(); i++)
	children[i]->emitHtml(out);

      /* Just process all of the children sequentially */
      break;
    }
  case DomNode::ntElem:
    {
      /* Make a temporary copy: */
      HtmlElementInfo hei = htmlElementInfo();

      if (! hei.wrap.empty() ) {
	out << "<" << hei.wrap;
	if (! hei.wclass.empty() )
	  out << " class=\"" << hei.wclass << "\"";
	out << ">";
      }

      out << "<" << hei.name;

      for (size_t i = 0; i < attrs.size(); i++)
	attrs[i]->emitHtml(out);

      for (size_t i = 0; i < children.size(); i++)
	if (children[i]->nodeType == DomNode::ntAttr)
	  children[i]->emitHtml(out);

      out << ">";

      out << hei.prefix;

      for (size_t i = 0; i < children.size(); i++)
	if (children[i]->nodeType != DomNode::ntAttr)
	  children[i]->emitHtml(out);

      out << "</" << hei.name << ">";

      if (! hei.wrap.empty() )
	out << "</" << hei.wrap << ">";

      break;
    }
  case DomNode::ntAttr: {
    {
      const AttrDomNode *an = 
	dynamic_cast<const AttrDomNode *> (this);
      assert(an);

      out << " " << name << "=" << an->value;

      break;
    }
  }
  case DomNode::ntText:
    {
      const TextDomNode *tn = 
	dynamic_cast<const TextDomNode *> (this);
      assert(tn);

      out << tn->text;

      break;
    }
  case DomNode::ntWhiteSpace:
    {
      const WhiteSpaceDomNode *wsn = 
	dynamic_cast<const WhiteSpaceDomNode *> (this);
      assert(wsn);

      out << wsn->text;

      break;
    }
  }

}

void
DomNode::emitXML(std::ostream& out) const
{
  switch(nodeType) {
  case DomNode::ntNone:
    {
      fprintf(stderr, "Internal error in DOM tree construction\n");
      exit(1);
    }
  case DomNode::ntRoot:
    {
      for (size_t i = 0; i < attrs.size(); i++)
	attrs[i]->emitXML(out);

      for (size_t i = 0; i < children.size(); i++)
	children[i]->emitXML(out);

      /* Just process all of the children sequentially */
      break;
    }
  case DomNode::ntElem:
    {
      out << "<" << name;

      for (size_t i = 0; i < attrs.size(); i++)
	attrs[i]->emitXML(out);

      for (size_t i = 0; i < children.size(); i++)
	if (children[i]->nodeType == DomNode::ntAttr)
	  children[i]->emitXML(out);

      out << ">";

      for (size_t i = 0; i < children.size(); i++)
	if (children[i]->nodeType != DomNode::ntAttr)
	  children[i]->emitXML(out);

      out << "</" << name << ">";

      break;
    }
  case DomNode::ntAttr: {
    {
      const AttrDomNode *an = 
	dynamic_cast<const AttrDomNode *> (this);
      assert(an);

      out << " " << name << "=" << an->value;

      break;
    }
  }
  case DomNode::ntText:
    {
      const TextDomNode *tn = 
	dynamic_cast<const TextDomNode *> (this);
      assert(tn);

      out << tn->text;

      break;
    }
  case DomNode::ntWhiteSpace:
    {
      const WhiteSpaceDomNode *wsn = 
	dynamic_cast<const WhiteSpaceDomNode *> (this);
      assert(wsn);

      out << wsn->text;

      break;
    }
  }

}
