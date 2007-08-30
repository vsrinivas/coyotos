#ifndef TOPSYM_HXX
#define TOPSYM_HXX

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

#include <libsherpa/GCPtr.hxx>
#include <libsherpa/Path.hxx>

using namespace sherpa;

struct TopSym : public Countable {
  std::string symName;
  GCPtr<Path> fileName;
  bool isUOC;			/* is this symbol a unit of compilation */
  bool isCmdLine;		/* is this a command-line UOC, as
				   opposed to something on the include
				   path? */

  TopSym(const std::string& s, GCPtr<Path> f, bool isCmdLine);
};

#endif /* TOPSYM_HXX */
