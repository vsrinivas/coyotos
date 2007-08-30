#ifndef OPTIONS_HXX
#define OPTIONS_HXX

/*
 * Copyright (C) 2005, The EROS Group, LLC.
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

#include <dirent.h>
#include <libsherpa/Path.hxx>
#include <libsherpa/CVector.hxx>

/* Flags set from command line option */
struct Options {
  static bool showParse;
  static bool showLex;
  static bool showPP;
  static bool useStdInc;
  static bool useStdLib;
  static bool showPasses;
  static bool ppFQNS;
  static std::string outputFileName;
  static std::string headerDir;
  static sherpa::CVector< sherpa::GCPtr<sherpa::Path> > libPath;
  static std::string targetArchName;

  static sherpa::GCPtr<sherpa::Path> resolveLibraryPath(std::string modName);
};

#endif /* OPTIONS_HXX */
