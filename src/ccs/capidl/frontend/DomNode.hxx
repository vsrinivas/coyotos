#ifndef DOMNODE_HXX
#define DOMNODE_HXX

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

#include <libsherpa/CVector.hxx>
#include <libsherpa/LexLoc.hxx>

struct DomNode {
  sherpa::LexLoc loc;
  std::string  name;

  enum {
    ntNone,
    ntRoot,
    ntElem,
    ntAttr,
    ntText,
    ntWhiteSpace
  } nodeType;

  struct HtmlElementInfo {
    std::string wrap;
    std::string wclass;
    std::string prefix;
    std::string name;
  };

  sherpa::CVector<DomNode *> attrs;
  sherpa::CVector<DomNode *> children;

  DomNode()
  {
    name = "<?>";
    loc = sherpa::LexLoc::Unspecified;
  }

  virtual ~DomNode();

  void dump(std::ostream&, int indent) const;

  // Following is implemented in DocComment.cxx, so that it will stay
  // in sync with the ElementInfo vector there.
  HtmlElementInfo htmlElementInfo() const;
  void emitHtml(std::ostream& out) const;
  void emitXML(std::ostream& out) const;

  inline void addChild(DomNode *child)
  {
    children.append(child);
  }

  inline void addAttr(DomNode *attr)
  {
    attrs.append(attr);
  }

  bool isEmpty() {
    return children.size() == 0;
  }
};

struct RootDomNode : DomNode {
  RootDomNode()
  {
    name = "<root>";
    nodeType = ntRoot;
  }

  ~RootDomNode();
};

struct ElemDomNode : DomNode {
  ElemDomNode(const sherpa::LexLoc& where, const std::string& s)
  {
    name = s;
    loc = where;
    nodeType = ntElem;
  }

  ~ElemDomNode();
};

struct AttrDomNode : DomNode {
  std::string  value;
  bool squote;

  AttrDomNode(const sherpa::LexLoc& where, const std::string& nm, 
	      const std::string& v, bool sq = false)
  {
    name = nm;
    loc = where;
    value = v;
    nodeType = ntAttr;
    squote = sq;
  }

  ~AttrDomNode();
};

struct TextDomNode : DomNode {
  std::string  text;

  TextDomNode(const sherpa::LexLoc& where, const std::string& s)
  {
    name = "<text>";
    loc = where;
    text = s;
    nodeType = ntText;
  }

  ~TextDomNode();
};

struct WhiteSpaceDomNode : TextDomNode {

  WhiteSpaceDomNode(const sherpa::LexLoc& where, const std::string& s)
    : TextDomNode(where, s)
  {
    nodeType = ntWhiteSpace;
  }

  ~WhiteSpaceDomNode();
};

#endif /* DOMNODE_HXX */
