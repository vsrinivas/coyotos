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
#include <string>
#include <iostream>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include "util.hxx"
#include "capidl.hxx"
#include "DocComment.hxx"

static const int debug = 0;
#define DEBUG(x) { if (debug) { std::cerr << x << std::endl; } }

static const struct doxygen_char_table {
  char key;
  const char *value;
} doxygen_chars[] = {
  { '\\', "\\" },
  { '@',  "@" },
  { '&',  "&amp;" },
  { '$',  "$" },
  { '#',  "#" },
  { '<',  "&lt;" },
  { '>',  "&gt;" },
  { '.',  "." },
  { 0, NULL}
};

static const char *doxygen_special_char(char c)
{
  for (int i = 0; doxygen_chars[i].key != 0; i++) {
    if (c == doxygen_chars[i].key)
      return (doxygen_chars[i].value);
  }
  return (NULL);
}

enum elem_type {
  html, autopar, doxy_autopar, doxy_noargs, doxy_wordarg
  // later: doxy_section
};

struct ElementInfo {
  const std::string element;
  const std::string autocloses;
  const enum elem_type type;
  const bool needsPara;

  ElementInfo(std::string _element, std::string _autocloses = "||",
	      enum elem_type _type = html, bool _needsPara = true) 
    : element(_element), autocloses(_autocloses),
      type(_type), needsPara(_needsPara)
  { }
};

static const struct ElementInfo elems[] = {
  ElementInfo("<<blank_line>>", "|doxygen:brief|doxygen:impl_p|"),
  ElementInfo("b"),
  ElementInfo("br"),
  ElementInfo("doxygen:b", "||", doxy_wordarg),
  ElementInfo("doxygen:brief","|doxygen:impl_p|", doxy_noargs, false),
  ElementInfo("doxygen:bug",
	      "|doxygen:impl_p|p|doxygen:brief|doxygen:note|", autopar, false),
  ElementInfo("doxygen:c", "||", doxy_wordarg),
  ElementInfo("doxygen:em", "||", doxy_wordarg),
  ElementInfo("doxygen:impl_p", "||", autopar), // must match parseAutoPar()
//  ElementInfo("doxygen:li", "|doxygen:li|doxygen:impl_p|", doxy_noargs),
  ElementInfo("doxygen:note",
	      "|doxygen:impl_p|p|doxygen:brief|doxygen:bug|", autopar, false),
  ElementInfo("doxygen:p", "||", doxy_wordarg),
  ElementInfo("em"),
  ElementInfo("li"),
  ElementInfo("ol"),
  ElementInfo("p", "|doxygen:impl_p|p|", html, false),
  ElementInfo("sup"),
  ElementInfo("table"),
  ElementInfo("tbody"),
  ElementInfo("td"),
  ElementInfo("tr"),
  ElementInfo("tt"),
  ElementInfo("ul"),
};

static int
elstrcmp(const void *vElem, const void *vCand)
{
  const std::string &key = ((const ElementInfo *)vElem)->element;
  const std::string &candidate = ((const ElementInfo *)vCand)->element;
  
  return key.compare(candidate);
}

static const ElementInfo *
findElementByTag(const std::string tag)
{
  const ElementInfo myel(tag);
  return (const ElementInfo *)bsearch(&myel, elems,
				      sizeof (elems)/sizeof(elems[0]),
				      sizeof(elems[0]),
				      elstrcmp);
}

static void 
validateElements()
{
  for (size_t i = 1; i < sizeof (elems)/sizeof(elems[0]); i++) {
    if (elstrcmp((void *)&elems[i-1], (void *)&elems[i]) >= 0) {
      std::cerr << "fatal setup error: elements \"" << elems[i-1].element <<
	"\" and \"" << elems[i].element << "\" are misordered." << std::endl;
      exit(1);
    }
  }
}

DocComment::DocComment()
  : comPos(comPosReal)
{
  root = NULL;
  validateElements();
}

sherpa::LexLoc 
DocComment::locOf(size_t pos) const
{
  sherpa::LexLoc ll = comStart;
  ll.updateWith(com.substr(0, pos));
  return ll;
}

void
DocComment::updateComPos(size_t newPos) 
{
  assert(comPos <= newPos);
  assert(newPos <= com.length());

  DEBUG("updateComPos: " << comPos << " -> " << newPos);

  if (comPos != newPos) {
    comPosReal = newPos;
    comAtBegOfLine = false;
  }
}

size_t
DocComment::processBegOfLine(size_t pos) const
{
  /*
   * If we hit the beginning of the line, skip any amount of whitespace,
   * any number of '*'s or '/'s, depending on the comment type, and then
   * a single, optional, whitespace.
   */
  size_t cur;
  char skipChar = (comType == CComment) ? '*' : '/';
  
  for (cur = pos; cur < com.length(); cur++)
    if (com[cur] != ' ' && com[cur] != '\t')
      break;
  for (; cur < com.length(); cur++)
    if (com[cur] != skipChar)
      break;
  if (cur < com.length() && (com[cur] == ' ' || com[cur] == '\t'))
    cur++;
  return (cur);
}

size_t
DocComment::skipWhiteSpace(size_t pos) const
{
  for (; pos < com.length(); pos++) {
again:
      char c = com[pos];
      if (c == '\r') {
	if (pos + 1 < com.length() && com[pos + 1] == '\n')
	  pos++;
	c = '\n';
      }
      if (c == '\n') {
	pos = processBegOfLine(pos + 1);
	goto again;
      }
      if (c != ' ' && c != '\t')
	break;
  }
  return (pos);
}

static inline bool
isIdentChar(char c) 
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	    (c >= '0' && c <= '9') || (c == '_'));
}

DomNode *
DocComment::parseText()
{
  enum input_type {
    typeWS, typePunct, typeIdentifier
  };
  enum input_type type;
  bool knowType = false;
  bool hitBegOfLine = false;
  bool possibleMixCase = true;
  bool sawInnerCapital = false;
  bool sawInnerDot = false;

  size_t cur;

  for (cur = comPos; cur < com.length(); cur++) {
    char c = com[cur];
    enum input_type newType;

    if (c == '>') {
      throw ParseFailureException(locOf(cur),
				  "Bare '>' in XML input. Perhaps &gt;?");
    }

    // If FIRST character is '<', this means that parseAhead failed to
    // find a legal element start/end. It is likely that this is a
    // bare '<'.
    if (c == '<' && (cur == comPos))
      throw ParseFailureException(locOf(cur),
				  "Invalid element or bare '<' in XML input. Perhaps &lt;?");

    if (c == '<' || c == '>' || c == '@')
      break;

    if (strchr(" \t\r\n", c) != NULL)
      newType = typeWS;
    else if (isIdentChar(c))
      newType = typeIdentifier;
    else if (cur != comPos && c == '.') {
      if (cur + 1 < com.length() && isIdentChar(com[cur + 1])) {
	newType = typeIdentifier;
	sawInnerDot = true;
      } else
	newType = typePunct;
    } else
      newType = typePunct;

    if (knowType && newType != type)
      break;
    
    knowType = true;
    type = newType;

    // mixed-case identifiers start with a letter, consist of only letters and
    // numerals, and have at least one inner capital letter
    if (type == typeIdentifier && possibleMixCase) {
      if (cur == comPos) {
	// must start with a letter
	if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z'))
	  possibleMixCase = false;
      } else {
	// must have a capital letter in the middle
	if (c >= 'A' && c <= 'Z')
	  sawInnerCapital = true;
      }
    }

    if (c == '\r') {
      if (cur < com.length() && com[cur + 1] == '\n')
	++cur;
      c = '\n';
    }
    if (c == '\n') {
      hitBegOfLine = true;
      cur++;
      break;   // process as if we finished, and return.
    }
  }
  
  if (!knowType) {
    assert(cur == comPos);
    return (NULL);
  }

  DomNode *ret;
  std::string text = com.substr(comPos, cur - comPos);
  if (type == typeWS)
    ret = new WhiteSpaceDomNode(locOf(comPos), text);
  else
    ret = new TextDomNode(locOf(comPos), text);

  DEBUG(((type == typeWS)? "WS " : "text ") << 
	comPos << "-" << cur << ": \"" << text << "\"");
	
  if (ret == NULL)
    throw ParseFailureException(locOf(comPos), "out of memory");

  // for mixed identifiers, wrap them in an appropriate Elem node.
  if (type == typeIdentifier && possibleMixCase &&
      (sawInnerCapital || sawInnerDot)) {
    std::string tag = (sawInnerDot) ? "capdoc:idlident" : "capdoc:mixident";
    DomNode *ret2 = new ElemDomNode(locOf(comPos), tag);
    if (ret == NULL) {
      throw ParseFailureException(locOf(comPos), "out of memory");
    }
    ret2->addChild(ret);
    ret = ret2;
  }
  updateComPos(cur);

  if (hitBegOfLine) {
    cur = processBegOfLine(comPos);
    updateComPos(cur);
    comAtBegOfLine = true;
  }

  return (ret);
}

inline bool validNameChar(char c, bool isFirstChar, bool isXml)
{
  // for now we cheat, and treat all non-ASCII characters as valid names
  // XML standard specifies:
  //   BaseChar:     [#x0041-#x005A] | [#x0061-#x007A] | [... >0x7f encodings]
  //   Digit:        [#x0030-#x0039] | [... >0x7f encodings]
  //
  //   Ideographics,CombiningChar,Extender: [... various >0x7f encodings]
  //
  //   NameChar:  Letter|Digit| '.' | '-' | '_' | ':' |CombiningChar|Extender
  //   Name:      (Letter | '_' | ':') (NameChar)*
  //
  //   Since we treat all >0x7f characters as valid name chars, we don't have
  //   to decode the UTF8.

  if ((c >= 0x41 && c <= 0x5a) || (c >= 0x61 && c <= 0x7a) ||
      c == '_' || (isXml && c == ':'))
    return true;
  if (!isFirstChar && ((c >= 0x30 && c <= 0x39) ||
		       (isXml && (c == '.' || c == '-'))))
    return true;
  return false;
}

enum name_type {
  XmlName,
  DoxygenName
};

bool
DocComment::parseGetName(size_t pos, enum DocComment::name_type type,
			 size_t &pos_out, std::string &name_out) 
  const
{
  bool isXml = (type == XmlName);
  size_t name_pos = pos;
  size_t end_pos;

  if (pos >= com.length())
    return (false);  // EOC
  if (!validNameChar(com[pos], true, isXml)) {
    return (false);
  }
  for (pos++;
       pos < com.length() && validNameChar(com[pos], false, isXml);
       pos++)
    ;
  end_pos = skipWhiteSpace(pos);

  // Doxygen names *must* have whitespace after them
  if (type == DoxygenName && pos == end_pos && pos < com.length())
    return (false);
      
  pos_out = end_pos;
  name_out = com.substr(name_pos, pos-name_pos);
  return true;
}

bool
DocComment::parseGetEntityName(size_t pos,
			       size_t &pos_out, std::string &name_out) const
{
  name_out = "";

  pos = skipWhiteSpace(pos);
  if (pos >= com.length())
    return (false);  // EOC
  return (parseGetName(pos, XmlName, pos_out, name_out));
}

struct DocComment::lookAhead
DocComment::parseAhead() const
{
  DEBUG("parseAhead: comPos: " << comPos << " com[comPos] == '" <<
	com[comPos] << "'");
  struct lookAhead ret;

  if (comPos == com.length()) {
    ret.eof = true;
    return ret;
  }
  ret.eof = false;
  ret.offset = com.npos;
  ret.info = NULL;

  char c = com[comPos];

  if (comAtBegOfLine) {
    size_t pos;
    for (pos = comPos; pos < com.length(); pos++) {
      char c = com[pos];
      if (c == ' ' || c == '\t')
	continue;
      if (c == '\r') {
	if (pos + 1 < com.length() && com[pos + 1] == '\n')
	  pos++;
	c = '\n';
      }
      if (c == '\n') {
	ret.type = la_emptyline;
	ret.tag = "<<blank_line>>";
	ret.offset = processBegOfLine(pos + 1);
	ret.info = findElementByTag(ret.tag);
	assert(ret.info != NULL);
	return ret;
      }
      break;          // non-whitespace before end of line
    }
  }
  
  switch (c) {
  case '<': {
    size_t pos = comPos;
    bool closeTag = false;
    
    if (pos >= com.length()) {
      /* parse failure; premature EOF */
      ret.eof = true;
      return ret;
    }
    if (com[pos + 1] == '/') {
      closeTag = true;
      pos++;
    }
    if (!parseGetEntityName(pos + 1, pos, ret.tag)) {
      throw ParseFailureException(locOf(comPos),
				  "unrecognized character in @ tag");
      ret.eof = true;
      return ret;
    }
    ret.type = (closeTag) ? la_close : la_open;

    pos = skipWhiteSpace(pos);
    size_t endOfTag = pos;

      // No attributes on close tag
    if (!closeTag) {
      for(;;) {
	std::string attrName, attrValue;

	pos = skipWhiteSpace(pos);

	if (com[pos] == '/' || com[pos] == '>')
	  break;

	if (!parseGetAttribute(pos, pos, attrName, attrValue))
	  break;
      }
    }
	   
    pos = skipWhiteSpace(pos);

    if (com[pos] != '/' && com[pos] != '>') {
      ret.type = la_other;
      return ret;
    }

    if (closeTag && com[pos] == '/') {
      ret.type = la_other;
      return ret;
    }

    if (com[pos] == '/') {
      pos = skipWhiteSpace(pos+1);
      if (pos >= com.length() || com[pos] != '>') {
	ret.type = la_other;
	return ret;
      }
    }
      
    if (com[pos] != '>') {
      ret.type = la_other;
      return ret;
    }

    ret.offset = closeTag ? (pos + 1) : endOfTag;
    ret.info = findElementByTag(ret.tag);
    if (ret.info == NULL)
      throw ParseFailureException(locOf(comPos),
				  "unrecognized tag <" + ret.tag + "...");   
      
    return ret;
  }
  case '@': {
    std::string name;
    if (comPos + 1 >= com.length())
      throw ParseFailureException(locOf(comPos), "missing @ tag name");

    if (!parseGetName(comPos + 1, DoxygenName, ret.offset, name)) {
      const char *newname;
      if ((newname = doxygen_special_char(com[comPos+1])) == NULL) {
	throw ParseFailureException(locOf(comPos),
				    "unrecognized character in @ tag");
      }

      ret.type = la_tagtext;
      ret.tag = newname;
      ret.offset = comPos + 2;
      return (ret);
    }
    ret.type = la_open;
    ret.tag = "doxygen:" + name;
    ret.info = findElementByTag(ret.tag);
    if (ret.info == NULL)
      throw ParseFailureException(locOf(comPos),
				  "unrecognized doxygen tag @" + name);   
    return ret;
  }

  default:
    ret.type = la_other;
    ret.offset = comPos;
    return ret;
  }
}

bool 
DocComment::parseGetAttribute(size_t pos,
			      size_t &pos_out, std::string &name_out,
			      std::string& attValue) const
{
  pos_out = pos;
  size_t curPos;

  attValue.clear();

  if (!parseGetEntityName(pos, curPos, name_out))
    return false;

  // parseGetName eats trailing whitespace, so no need to do so here.

  if (pos >= com.length() || com[pos] != '=')
    return false;

  pos = skipWhiteSpace(pos+1);
  size_t endPos = pos;

  if (com[endPos] == '"' || com[endPos] == '\'') {
    char lookFor = com[endPos++];
    while (endPos < com.length() && com[endPos] != lookFor)
      endPos++;
    if (endPos >= com.length())
      return false;
    endPos++;

    attValue = com.substr(pos, endPos - pos);
    pos_out = endPos;
  }
  else {
    while (endPos < com.length() && validNameChar(com[endPos], false, true))
      endPos++;
    if (endPos >= com.length())
      return false;

    attValue = '"' + com.substr(pos, endPos - pos) + '"';
    pos_out = endPos;
  }

  return true;
}

bool
DocComment::parseDocAttributes(DomNode *elem)
{
  DEBUG("parseDocAttributes");

  while (comPos < com.length()) {
    size_t pos, valuePos;
    std::string attName, attValue;
    bool needQuotes = false;

    if (com[comPos] == '/' || com[comPos] == '>')
      return true;

    if (!parseGetName(comPos, XmlName, pos, attName))
      throw ParseFailureException(locOf(comPos),
				  "invalid character in attributes of <" +
				  elem->name + "...");

    if (pos >= com.length() || com[pos] != '=')
      throw ParseFailureException(locOf(comPos),
				  "missing '=' in attributes of  <" +
				  elem->name + "...");

    valuePos = pos = skipWhiteSpace(pos + 1);
    if (pos >= com.length())
      break;  // hit EOF

    if (com[pos] == '"' || com[pos] == '\'') {
      char lookfor = com[pos];
      while (++pos < com.length()) {
	if (com[pos] == lookfor) {
	  pos++;
	  break;
	}
      }
    } else {
      needQuotes = true;
      while (pos < com.length() && validNameChar(com[pos], false, true))
	pos++;
      if (pos >= com.length()) {
	break;
      }
    }

    if (pos >= com.length()) {
      break;
    }
    attValue = com.substr(valuePos, pos - valuePos);
    if (needQuotes)
      attValue = '"' + attValue + '"';

    DomNode *newNode = new AttrDomNode(locOf(comPos), attName, attValue);
    if (newNode == NULL)
      throw ParseFailureException(locOf(comPos), "out of memory");

    elem->addAttr(newNode);

    pos = skipWhiteSpace(pos);
    updateComPos(pos);
  }
  throw ParseFailureException(locOf(comPos), std::string(
			      "Reached end of comment processing ") +
			      "attributes of  <" + elem->name + "...");
}

DomNode *
DocComment::parseDocNode(struct DocComment::lookAhead la)
{
  DomNode *newNode = new ElemDomNode(locOf(comPos), la.tag);
  bool hasBody = true;
  bool autoPar = false;

  DEBUG("parseDocNode: " << la.tag);

  updateComPos(la.offset);
  
  switch (la.info->type) {
  case html: {
    if (!parseDocAttributes(newNode)) {
      delete newNode;
      return (NULL);
    }
    if (comPos < com.length() && com[comPos] == '/') {
      hasBody = false;
      updateComPos(comPos + 1);
    }
    if (comPos >= com.length() || com[comPos] != '>')
      throw ParseFailureException(locOf(comPos), "missing '>' in <" + la.tag +
				  " ...");

    updateComPos(comPos + 1);
    break;
  }
  case doxy_wordarg: {
    size_t pos = comPos;
    for (; pos < com.length(); pos++)
      if (strchr(" \t\r\n", com[pos]) != NULL)
	break;
    if (pos == com.length())
      throw ParseFailureException(locOf(comPos), "@ tag requires argument");

    DomNode *arg = new TextDomNode(locOf(comPos), com.substr(comPos, pos - comPos));
    if (arg == NULL)
      throw ParseFailureException(locOf(comPos), "out of memory");

    updateComPos(pos);
    newNode->addChild(arg);
    hasBody = false;

    break;
  }
  case doxy_autopar:
    autoPar = true;
    hasBody = true;
    break;

  case doxy_noargs:
    hasBody = true;
    break;
 
  case autopar:
    hasBody = true;
    autoPar = false;
    break;
  }
  if (hasBody)
    parseDocMain(newNode, autoPar, false);
  return (newNode);
}

DomNode *
DocComment::parseAutoPar()
{
  struct DocComment::lookAhead la;
  DEBUG("parseAutoPar");
  
  la.eof = false;
  la.offset = comPos;
  la.type = la_open;
  la.tag = "doxygen:impl_p";  // *MUST* match ElementInfo
  la.info = findElementByTag(la.tag);
  
  assert(la.info != NULL);

  return (parseDocNode(la));
}

void
DocComment::parseDocMain(DomNode *current, bool autoPar, bool isRoot) 
{
  DEBUG("parseDocMain");
  for (;;) {
    struct lookAhead la = parseAhead();

    DEBUG("parseAhead: eof: " << la.eof << " type: " << la.type <<
	  " tag: " << la.tag << " offset: " << la.offset);    
    
    if (la.eof)
      return;
    switch (la.type) {
    case la_close: {
      if (la.tag == current->name) {
	updateComPos(la.offset);
	return;                  // proper close
      }
      if (isRoot) {
	throw ParseFailureException(locOf(comPos),
				    "</"+la.tag+"> doesn't match anything");
      }
      return;
    }
    case la_emptyline:
    case la_open: {
      std::string autocloses = (la.info != NULL)? la.info->autocloses : "";

      if (autoPar && la.info != NULL && la.info->needsPara) {
	DomNode *newNode = parseAutoPar();
	current->addChild(newNode);
	continue;
      }
      if (autocloses.find("|" + current->name + "|") != autocloses.npos) {
	if (la.type == la_emptyline) {
	  comAtBegOfLine = false;
	  updateComPos(la.offset);  // consume the blank line
	}
	return;   // we've been autoclosed
      }

      if (la.type == la_emptyline) {
	comAtBegOfLine = false;      // turn off <<blank-line>> tags and retry
	continue;
      }
      DEBUG(">>>");
      DomNode *newNode = parseDocNode(la);
      DEBUG("<<<");
      current->addChild(newNode);
      continue;
    }
    case la_tagtext: {
      DomNode *newNode;
      if (autoPar)
	newNode = parseAutoPar();
      else {
	newNode = new TextDomNode(locOf(comPos), la.tag);
	if (newNode == NULL)
	  throw ParseFailureException(locOf(comPos), "out of memory");
	updateComPos(la.offset);
      }
      current->addChild(newNode);
      continue;
    }
    case la_other: {
      DEBUG(">>>");
      DomNode *newNode = autoPar ? parseAutoPar() : parseText();
      DEBUG("<<<");
      current->addChild(newNode);
      continue;
    }
    default:
      assert(0);
      break;
    }
  }
}

const std::string
DocComment::asString() const
{
  std::string result;
  size_t offset = 0;
  
  // we use our beggining-of-line processing to strip all of the line-heading
  // characters from the string
  while ((offset = processBegOfLine(offset)) < com.length()) {
    size_t n;
    
    for (n = offset; n < com.length(); n++) {
      char c = com[n];
      if (c == '\r') {
	c = '\n';
	if (n + 1 < com.length() && com[n+1] == '\n')
	  n++;
      }
      if (c == '\n')
	break;
    }
    result.append(com.substr(offset, n-offset + 1));

    offset = n + 1;   // skip over the processed string
  }
  return (result);
}

void
DocComment::ProcessComment(LexLoc cHere, std::string c, enum commentType type)
{
  DEBUG(c << "+++");
  if (type == CComment) {

    // strip off the leading '/' and trailing '*/'

    assert(c.length() > 4 && c.substr(0,2) == "/*" ||
	   c.substr(c.length()-2,2) == "*/");
    c = c.substr(1,c.length()-3);
    cHere.updateWith("/");
  }

  // load the comment info into the shared variables; this way, we don't have
  // to pass them around.
  comType = type;
  com = c;
  comPosReal = 0;
  comStart = cHere;

  size_t pos = skipWhiteSpace(processBegOfLine(0));
  if (pos > 1)
    updateComPos(pos);

  root = new RootDomNode();
  parseDocMain(root, true, true);
}

int
DocComment::briefDocOffset() const
{
  // Find the "@brief" node;  it must only have whitespace (or 
  // auto-paragraphs of whitespace) before it, and be a child of the
  // root node.

  for (size_t i = 0; i < root->children.size(); i++) {
    if (root->children[i]->nodeType == DomNode::ntWhiteSpace)
      continue;
    if (root->children[i]->nodeType == DomNode::ntElem) {
      ElemDomNode *en = dynamic_cast<ElemDomNode *>(root->children[i]);

      if (en->name == "doxygen:brief")
	return i;
      else if (en->name == "doxygen:impl_p") {
	for (size_t j = 0; j < en->children.size(); j++) {
	  if (en->children[j]->nodeType != DomNode::ntWhiteSpace)
	    return (-1);
	}
	continue;
      }
    }
    break;
  }
  return (-1);
}

bool
DocComment::HasBriefDoc() const
{
  return (briefDocOffset() != -1);
}

DomNode *
DocComment::GetBriefDoc() const
{
  DomNode *dn = new RootDomNode();

  int i = briefDocOffset();
  if (i != -1) {
    DomNode *en = root->children[i];
    dn->children.append(en->children);
  }
      
  return dn;
}

bool
DocComment::HasFullDoc() const
{
  if (root->isEmpty())
    return false;

  int i = briefDocOffset();

  if (i != -1)
    return root->children.size() > 1; // must have other children

  // Otherwise, the first thing must be part of a full comment:
  return true;
}

DomNode *
DocComment::GetFullDoc() const
{
  int i = briefDocOffset();

  if (i != -1) {
    DomNode *dn = new RootDomNode();
    
    dn->children.append(root->children);
    dn->children.remove(i);

    return dn;
  }
      
  return root;
}
