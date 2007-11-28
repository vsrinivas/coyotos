#ifndef DOCCOMMENT_HXX
#define DOCCOMMENT_HXX

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

#include <libsherpa/Path.hxx>
#include <libsherpa/LexLoc.hxx>
#include "HtmlParseType.hxx"
#include "LTokString.hxx"

class ElementInfo;

class DocComment {
public:
  enum commentType {
    CComment,
    CxxComment
  };

  struct ParseFailureException {
    sherpa::LexLoc loc;
    std::string error;

    ParseFailureException(sherpa::LexLoc _loc, std::string _error) 
    {
      loc = _loc;
      error = _error;
    }
  };

  DocComment();

  void ProcessComment(sherpa::LexLoc, std::string, enum commentType);

  bool HasBriefDoc() const;
  bool HasFullDoc() const;
  DomNode *GetBriefDoc() const;
  DomNode *GetFullDoc() const;

  const std::string asString() const;

private:
  DomNode *root;

  enum lookAheadType { la_emptyline, la_open, la_close, la_tagtext, la_other };

  struct lookAhead {
    bool eof;
    std::string tag;
    enum lookAheadType type;
    size_t offset;   // offset after the above information
    const ElementInfo *info;
  };

  enum commentType comType;
  std::string com;
  const size_t &comPos;
  size_t comPosReal;
  sherpa::LexLoc comStart;
  bool comAtBegOfLine;

  enum name_type {
    XmlName,
    DoxygenName
  };

  sherpa::LexLoc locOf(size_t pos) const;

  size_t processBegOfLine(size_t) const;
  size_t skipWhiteSpace(size_t) const;
  bool parseGetName(size_t, enum name_type, size_t &, std::string &) const;
  bool parseGetEntityName(size_t, size_t &, std::string &) const;
  struct lookAhead parseAhead() const;

  void updateComPos(size_t);
  DomNode *parseText();
  bool parseDocAttributes(DomNode *);
  DomNode *parseDocNode(struct lookAhead la);
  DomNode *parseAutoPar();
  void parseDocMain(DomNode *, bool, bool);

  int briefDocOffset() const;
};

#endif /* DOCCOMMENT_HXX */
