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

#include "DocComment.hxx"
#include "INOstream.hxx"

static inline int
max(int a, int b)
{
  return (a >= b) ? a : b;
}

static inline unsigned
min(unsigned a, unsigned b)
{
  return (a <= b) ? a : b;
}

static inline
unsigned round_up(unsigned value, unsigned to)
{
  value += (to - 1);
  value -= value % to;
  return value;
}

void do_indent(std::ostream&, int indent);
void PrintDocComment(const DocComment *dc, std::ostream& out, int indent);
void PrintDocComment(const DocComment *dc, INOstream& out);
bool isMixedCase(const std::string& nm);
bool isSameCase(const std::string& nm);
std::string toUpperCase(const std::string& nm);
std::string toLowerCase(const std::string& nm);
bool tagNameEqual(const std::string& s1, const std::string& s2, bool eitherCase);
