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

#include <sys/types.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <string>
#include <fstream>
#include <sstream>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include <libsherpa/Path.hxx>
#include <libsherpa/Logging.hxx>

#include "SymTab.hxx"
#include "IdlParseType.hxx"
#include "PrescanLexer.hxx"
#include "IdlLexer.hxx"
#include "ArchInfo.hxx"
#include "backend.hxx"
#include "capidl.hxx"
#include "util.hxx"

bool showparse = false;
bool opt_debug_encodings = false;
bool opt_index = false;

CVector<std::string> searchPath;
GCPtr< CVector<GCPtr<TopSym> > >uocMap = 0;

/** @brief Accept file names vs. symbol names.
 *
 * This is an experimental ifdef. Leave it alone unless you are
 * experimenting.
 *
 * If defined, capidl wants file names as its arguments. If not
 * defined, capidl wants symbol names.
 */
#define USE_FILENAMES


void
pkgwalker(GCPtr<Symbol> scope, BackEndFn outfn)
{
  /* Export subordinate packages first! */
  for (size_t i = 0; i < scope->children.size(); i++) {
    GCPtr<Symbol> child = scope->children[i];
    if (child->cls != sc_package)
      outfn(child);

    if (child->cls == sc_package)
      pkgwalker(child, outfn);
  }
}

void
parse_file(GCPtr<Path> fileName, bool isCmdLine)
{
  std::ifstream fin(fileName->c_str());

  if (!fin.is_open()) {
    fprintf(stderr, "Couldn't open description file \"%s\"\n",
	    fileName->c_str());
    exit(1);
  }

  GCPtr<Symbol> usingScope = Symbol::CurScope;
  Symbol::CurScope = 0;

  IdlLexer lexer(fin, fileName, isCmdLine);
  if (showparse)
    lexer.setDebug(showparse);

  {
    extern int idlparse(IdlLexer *);
    idlparse(&lexer);
  }
  
  fin.close();

  if (lexer.NumErrors() != 0u)
    exit(1);

  Symbol::CurScope = usingScope;
}

unsigned
contains(const std::string& scope, const std::string& sym)
{
  char nextc;

  unsigned scopeLen = strlen(scope.c_str());

  if (scopeLen > strlen(sym.c_str()))
    return 0;

  if (scope.substr(0, scopeLen) != sym.substr(0, scopeLen))
    return 0;

  nextc = sym[scopeLen];

  if (nextc == '.' || nextc == 0)
    return scopeLen;
  return 0;
}

void
import_uoc(const std::string& ident)
{
  GCPtr<Symbol> sym = Symbol::UniversalScope->LookupChild(ident, 0);

  if (sym) {
    /* If we find the symbol and it is marked done, we have already
       loaded this unit of compilation and we can just return. */
    if (sym->complete)
      return;
    
    fprintf(stderr, "Recursive dependency on \"%s\"\n",
	    ident.c_str());
    exit(1);

  }

  for (size_t i = 0; i < uocMap->size(); i++) {
    GCPtr<TopSym> ts = uocMap->elem(i);
    std::string scopeName = ts->symName;
    
    if (contains(scopeName,ident)) {
      parse_file(ts->fileName, ts->isCmdLine);
      return;
    }
  }
}

GCPtr<Path> 
lookup_containing_file(GCPtr<Symbol> s)
{
  std::string uocName;

  s = s->UnitOfCompilation();
  uocName = s->QualifiedName('.');

  for (size_t i = 0; i < uocMap->size(); i++) {
    GCPtr<TopSym> ts = uocMap->elem(i);
    
    if(ts->symName == uocName)
      return ts->fileName;
  }

  std::cerr << "NOT FOUND!" << std::endl;
  return new Path();
}

/** Locate the containing UOC for a symbol and import it.

    Our handling of packages is slightly different from that of Java,
    with the consequence that there isn't any ambiguity about
    containership nesting. Every file exports a single top-level name,
    and we are checking here against the top-level names rather than
    against the package names.

    As a FUTURE optimization, we will use the isUOC field in the
    TopsymMap to perform lazy file prescanning.
*/

void
import_symbol_uoc(const std::string& ident)
{
  /* First, see if the symbol is defined in one of the input files. 
     This is true exactly if the symbol provided by the input file is
     a identifier-wise substring of the desired symbol. */ 

  for (size_t i = 0; i < uocMap->size(); i++) {
    GCPtr<TopSym> ts = uocMap->elem(i);
    std::string scopeName = ts->symName;
    
    if (contains(scopeName,ident))
      import_uoc(scopeName);
  }
}

// #define PRESCAN_BUGHUNT
void
prescan(GCPtr<Path> fileName, bool isCmdLine)
{
  std::ifstream fin(fileName->c_str());

  if (!fin.is_open()) {
    fprintf(stderr, "Couldn't open description file \"%s\"\n",
	    fileName->c_str());
    exit(1);
  }

  PrescanLexer lexer(fin, fileName, uocMap, isCmdLine);
  if (showparse)
    lexer.setDebug(showparse);

#ifdef PRESCAN_BUGHUNT
  if (showparse) {
    fprintf(stderr, "BEGIN PRESCAN PARSE\n");
    fflush(stderr);
    std::cout.flush();
  }
#endif
  (void) lexer.lex();
#ifdef PRESCAN_BUGHUNT
  if (showparse) {
    fprintf(stderr, "END PRESCAN PARSE\n");
    fflush(stderr);
    std::cout.flush();
  }
#endif
  fin.close();
}

void
prescan_includes(GCPtr<Path> dirPath)
{
  GCPtr<OpenDir> dir;
  try {
    dir = dirPath->openDir(true);
  }
  catch (...) {
    std::cerr << "Directory \"" << *dirPath << "\" not found." << endl;
    throw;
  }

  for (;;) {
    std::string de = dir->readDir();

    if (de.empty())
      break;

    if (Path::shouldSkipDirent(de))
      continue;

    GCPtr<Path> entPath = new Path(*dirPath + Path(de));
    
    {
      if (entPath->isDir()) {
	prescan_includes(entPath);
	continue;
      }

      std::string::size_type dotPos = entPath->asString().rfind('.');

      if (dotPos == std::string::npos) // not a .idl file
	continue;
	
      std::string suffix = entPath->asString().substr(dotPos);
      if (suffix != ".idl") // not a .idl file
	continue;

      prescan(entPath, false);
    }
  }
}

Path outputFileName;
Path target(".");

/* Option processing: */
#define LOPT_ENCODINGS  257   /* Show positional encodings */
struct option longopts[] = {
  { "debug-encodings",      0,  0, LOPT_ENCODINGS },
  
  /* Options that have short-form equivalents: */
  { "architecture",         1,  0, 'a' },
  { "debug",                0,  0, 'd' },
  { "headers",              1,  0, 'h' },
  { "include",              1,  0, 'I' },
  { "index"  ,              1,  0, 'n' },
  { "language",             1,  0, 'l' },
  { "outdir",               1,  0, 'D' },
  { "output",               1,  0, 'o' },
  { "server-headers",       0,  0, 's' },
  { "template",             0,  0, 't' },
  { "verbose",              0,  0, 'v' },
  {0,                       0,  0, 0}
};

int
main(int argc, char *argv[])
{
  int c;
  extern int optind;
#if 0
  extern char *optarg;
#endif
  BackEnd *be;
  int opterr = 0;
  std::string lang = "c-client-header";
  bool verbose = false;
  
  // FIX: Why wasn't this covered by the 'using' declaration?
  sherpa::appName = "capidl";

  while ((c = getopt_long(argc, argv, 
			  "a:tshnD:A:dvl:o:I:" /* "x:" */,
			  longopts, 0
		     )) != -1) {

    switch(c) {
    case 'v':
      verbose = true;
      break;

    case 'D':
      target = Path(optarg).canonical();
      break;

    case 'l':
      lang = optarg;
      break;

    case 'a':
      targetArch = findArch(optarg);
      if (targetArch == 0) {
	fprintf(stderr, "Requested target architecture not found\n");
	exit(1);
      }
      break;

    case 'o':
      outputFileName = Path(optarg);
      break;

    case 'I':
      searchPath.append(optarg);
      break;

    case 'd':
      showparse = true;
      break;

    case 's':
      lang = "c-server-header";
      break;

    case LOPT_ENCODINGS:
      opt_debug_encodings = true;
      break;

    case 'h':
      lang = "c-client-header";
      break;

    case 't':
      lang = "c-template";
      break;

    case 'n':
      {
	opt_index = true;
	break;
      } 

    default:
      opterr++;
    }
  }
  
  argc -= optind;
  argv += optind;
  
  if (argc == 0)
    opterr++;

#if 0
  if (target == 0)
    opterr++;
#endif

  if (opterr) {
    fprintf(stderr,
	    "Usage: capidl -a target-arch -D output-dir\n"
	    "  [-c | -s | -t | --l output-language] "
	    "  [-v] [-d] [-nostdinc] [-Idir] "
	    "  [-o output-file [-n]\n"
	    "  idl_files\n");
    exit(1);
  }

  be = resolve_backend(lang);

  if (be == 0) {
    fprintf(stderr, "capidl: Output language \"%s\" is not recognized.\n",
            lang.c_str());
    exit(1);
  }

  Symbol::InitSymtab();

  uocMap = new CVector<GCPtr<TopSym> >;

#ifdef USE_FILENAMES
  /* Must prescan the command line units of compilation first, so that
     they get marked as units of compilation. */
  for (int i = 0; i < argc; i++)
    prescan(new Path(argv[i]), true);
#endif

  for (size_t i = 0; i < searchPath.size(); i++)
    prescan_includes(new Path(searchPath[i]));

  for (size_t i = 0; i < uocMap->size(); i++) {
    GCPtr<TopSym> ts = uocMap->elem(i);
    std::string ident = ts->symName;

    import_uoc(ident);
  }

  Symbol::UniversalScope->QualifyNames();

  if (!Symbol::UniversalScope->ResolveReferences()) {
    fprintf(stderr, "capidl: Symbol reference resolution could not be completed.\n");
    exit(1);
  }

  Symbol::UniversalScope->ResolveIfDepth();

  if (!Symbol::UniversalScope->TypeCheck()) {
    fprintf(stderr, "capidl: Type errors are present.\n");
    exit(1);
  }

  if (be->typecheck && !be->typecheck(Symbol::UniversalScope)) {
    fprintf(stderr, "capidl: Target-specific type errors are present.\n");
    exit(1);
  }

  if (!Symbol::UniversalScope->IsLinearizable()) {
    fprintf(stderr, "capidl: Circular dependencies are present.\n");
    exit(1);
  }

#ifndef USE_FILENAMES
  /* Must prescan the command line units of compilation first, so that
     they get marked as units of compilation. */
  for (int i = 0; i < argc; i++) {
    GCPtr<Symbol> s = Symbol::UniversalScope->LookupChild(argv[i], 0);
    if (!s) {
      fprintf(stderr, "Symbol %s not found\n", argv[i]);
      exit(0);
    }

    s->isActiveUOC = true;
  }
#endif

  if (be->xform) be->xform(Symbol::UniversalScope);

  if (be->scopefn)
    be->scopefn(Symbol::UniversalScope, be->fn);
  else
    pkgwalker(Symbol::UniversalScope, be->fn);
  
  exit(0);
}
