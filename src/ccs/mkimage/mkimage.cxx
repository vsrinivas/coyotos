/*
 * Copyright (C) 2007, The EROS Group, LLC.
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

#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


#include <fstream>
#include <getopt.h>
#include <langinfo.h>

#include <libsherpa/Path.hxx>
#include <libsherpa/UExcept.hxx>
#include <libsherpa/avl.hxx>
#include <iostream>
#include <sstream>

#include "Version.hxx"
#include "AST.hxx"
#include "Environment.hxx"
#include "UocInfo.hxx"
#include "Options.hxx"

using namespace sherpa;
using namespace std;

bool Options::showLex = false;
bool Options::showParse = false;
bool Options::useStdInc = true;
bool Options::useStdLib = true;
bool Options::showPasses = false;
bool Options::ppFQNS = false;
std::string Options::outputFileName;
std::string Options::headerDir;
CVector< GCPtr<sherpa::Path> > Options::libPath;
std::string Options::targetArchName;
  

#define LOPT_SHOWLEX      257   /* Show tokens */
#define LOPT_SHOWPARSE    258   /* Show parse */
#define LOPT_SHOWPASSES   259   /* Show processing passes */
#define LOPT_DUMPAFTER    260   /* PP after this pass */
#define LOPT_NOSTDINC     261   /* Do not append std search paths */
#define LOPT_NOSTDLIB     262   /* Do not append std lib paths */
#define LOPT_PPFQNS       263   /* Show FQNs when pretty printing */

struct option longopts[] = {
  { "dumpafter",            1,  0, LOPT_DUMPAFTER },
  { "headers",              1,  0, 'H' },
  { "help",                 0,  0, 'h' },
  { "nostdinc",             0,  0, LOPT_NOSTDINC },
  { "nostdlib",             0,  0, LOPT_NOSTDLIB },
  { "ppfqns",               0,  0, LOPT_PPFQNS },
  { "showlex",              0,  0, LOPT_SHOWLEX },
  { "showparse",            0,  0, LOPT_SHOWPARSE },
  { "showpasses",           0,  0, LOPT_SHOWPASSES },
  { "target",               1,  0, 't' },
  { "version",              0,  0, 'V' },
  {0,                       0,  0, 0}
};

/// @brief Search for an executable using the library path
GCPtr<Path> 
Options::resolveLibraryPath(std::string exec)
{
  Path ifPath(exec);

  if (ifPath.isEmpty())
    return NULL;

  // absolute paths don't get expanded.
  if (ifPath.isAbsolute())
    return new Path(ifPath);

  for (size_t i = 0; i < Options::libPath.size(); i++) {
    Path testPath = *Options::libPath[i] + ifPath;
    
    if (testPath.exists() && testPath.isFile())
      return new Path(testPath);
  }

  return NULL;
}

void
help()
{
  std::cerr 
    << "Usage:" << endl
    << "  mkimage -t targetArch [-o outfile] [-I moddir] modname\n"
    << "      Interprets the module \"modname\"\n"
    << "  mkimage -t targetArch [-o outfile] [-I moddir] [-H hdrdir] modname\n"
    << "      Generates an export header file for enums in \"modname\"\n"
    << "  mkimage -V|--version\n"
    << "  mkimage -h|--help\n"
    << "Debugging options:\n"
    << "  --showlex --showparse\n" 
    << flush;
}
 
void
fatal()
{
  cerr << "Confused due to previous errors, bailing."
       << endl;
  exit(1);
}

int
main(int argc, char *argv[]) 
{
  int c;
  extern int optind;
  int opterr = 0;

  while ((c = getopt_long(argc, argv, 
			  "o:H:VhI:L:t:",
			  longopts, 0
		     )) != -1) {
    switch(c) {
    case 'V':
      cerr << "mkimage Version: " << MKIMAGE_VERSION << endl;
      exit(0);
      break;

    case LOPT_NOSTDINC:
      Options::useStdInc = false;
      break;

    case LOPT_NOSTDLIB:
      Options::useStdLib = false;
      break;

    case LOPT_SHOWPARSE:
      Options::showParse = true;
      break;

    case LOPT_SHOWLEX:
      Options::showLex = true;
      break;

    case LOPT_SHOWPASSES:
      Options::showPasses = true;
      break;

    case LOPT_DUMPAFTER:
      {
	for (size_t i = (size_t)pn_none+1; i < (size_t) pn_npass; i++) {
	  if (strcmp(UocInfo::passInfo[i].name, optarg) == 0 ||
	      strcmp("ALL", optarg) == 0)
	    UocInfo::passInfo[i].printAfter = true;
	}

	break;
      }

    case LOPT_PPFQNS:
      Options::ppFQNS = true;
      break;

    case 'h': 
      {
	help();
	exit(0);
      }

    case 'o':
      Options::outputFileName = optarg;
      break;

    case 'H':
      Options::headerDir = optarg;
      break;

    case 'I':
      UocInfo::searchPath.append(new Path(optarg));
      break;

    case 'L':
      Options::libPath.append(new Path(optarg));
      break;

    case 't':
      Options::targetArchName = optarg;
      break;

    default:
      opterr++;
      break;
    }
  }
  
  const char *root_dir = getenv("COYOTOS_ROOT");
  const char *xenv_dir = getenv("COYOTOS_XENV");

  if (root_dir) {
    stringstream incpath;
    stringstream libpath;
    incpath << root_dir << "/usr/include/mki";
    libpath << root_dir << "/usr/domain";

    if (Options::useStdInc)
      UocInfo::searchPath.append(new Path(incpath.str()));
    if (Options::useStdLib)
      Options::libPath.append(new Path(libpath.str()));
  }

  if (xenv_dir) {
    stringstream incpath;
    stringstream libpath;
    incpath << xenv_dir << "/usr/include/mki";
    libpath << xenv_dir << "/usr/domain";

    if (Options::useStdInc)
      UocInfo::searchPath.append(new Path(incpath.str()));
    if (Options::useStdLib)
      Options::libPath.append(new Path(libpath.str()));
  }

  argc -= optind;
  argv += optind;
  
  if (argc == 0)
    opterr++;

  if (opterr) {
    std::cerr << "Usage: Try mkimage --help" << std::endl;
    exit(0);
  }

  try {
    if (Options::outputFileName.size() == 0)
      Options::outputFileName = "mkimage.out";

    GCPtr<CoyImage> ci = new CoyImage(Options::targetArchName);

    uint32_t compileVariant = 
      Options::headerDir.size() ? CV_CONSTDEF : CV_INTERP;

    // Compile everything on the command line:
    LexLoc cmdLoc(new Path("<command line>"), 0, 0);
    for(int i = 0; i < argc; i++) {
      GCPtr<UocInfo> uoc = 
	UocInfo::importModule(std::cerr, cmdLoc, ci, argv[i], compileVariant);
      uoc->isCmdLine = true;
    }

    if (Options::headerDir.size() == 0)
      ci->ToFile(Path(Options::outputFileName));
  }
  catch ( const sherpa::UExceptionBase& ex ) {
    std::cerr << "Exception "
	      << ex.et->name
	      << " at "
	      << ex.file
	      << ":"
	      << ex.line
	      << ": "
	      << ex.msg
	      << std::endl;
  }

  exit(0);
}
