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

#include <stdio.h>
#include <dirent.h>

#include <iostream>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include "util.hxx"

void
do_indent(std::ostream& out, int indent)
{
  int i;

  for (i = 0; i < indent; i ++)
    out << ' ';
}

void
PrintDocComment(const DocComment *dc, std::ostream& out, int indent)
{
  bool want_fill = true;
  const char *s = dc->asString().c_str();

  do_indent(out, indent);
  out << "/**\n";

  while (*s) {
    if (want_fill) {
      do_indent(out, indent + 1);
      out << "* ";
      want_fill = false;
    }

    if (*s == '\n')
      want_fill = true;
    out << *s++;
  }

  do_indent(out, indent);
  out << "*/\n";
}

void
PrintDocComment(const DocComment *dc, INOstream& out)
{
  bool bol = true;
  const char *s = dc->asString().c_str();

  out << "/**\n";
  out.indent(1);

  while (*s) {
    if (bol) {
      out << "* ";
      bol = false;
    }

    if (*s == '\n')
      bol = true;
    out << *s++;
  }

  out << "*/\n";
  out.indent(-1);
}

bool 
isMixedCase(const std::string& nm)
{
  /* We don't accept mixed-case tags */
  bool allLower = true;
  bool allUpper = true;

  for (std::string::size_type pos = 0; pos < nm.size(); pos++) {
    if (isupper(nm[pos]))
      allLower = false;
    if (islower(nm[pos]))
      allUpper = false;
  }

  return !(allLower || allUpper);
}

bool 
isSameCase(const std::string& nm)
{
  /* We don't accept mixed-case tags */
  bool allLower = true;
  bool allUpper = true;

  for (std::string::size_type pos = 0; pos < nm.size(); pos++) {
    if (isupper(nm[pos]))
      allLower = false;
    if (islower(nm[pos]))
      allUpper = false;
  }

  return (allLower || allUpper);
}

std::string
toUpperCase(const std::string& tag)
{
  std::string out = tag;

  for (std::string::size_type pos = 0; pos < out.size(); pos++) {
    if (islower(out[pos]))
      out[pos] = toupper(out[pos]);
  }

  return out;
}

std::string
toLowerCase(const std::string& tag)
{
  std::string out = tag;

  for (std::string::size_type pos = 0; pos < out.size(); pos++) {
    if (isupper(out[pos]))
      out[pos] = tolower(out[pos]);
  }

  return out;
}

bool
tagNameEqual(const std::string& t1, const std::string& t2, bool eitherCase)
{
  if (eitherCase && isSameCase(t1) && isSameCase(t2))
    return toUpperCase(t1) == toUpperCase(t2);

  return t1 == t2;
}

