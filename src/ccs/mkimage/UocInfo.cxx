/*
 * Copyright (C) 2006, The EROS Group, LLC.
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
#include <stdint.h>
#include <dirent.h>
#include <string>
#include <iostream>
#include <sstream>
#include "UocInfo.hxx"
#include "Options.hxx"
#include "Lexer.hxx"

using namespace sherpa;

CVector<GCPtr<Path> > UocInfo::searchPath;
AvlMap<std::string, GCPtr<UocInfo> > UocInfo::uocList;
std::vector<UocInfo::ImportRecord> UocInfo::importStack;

PassInfo UocInfo::passInfo[] = {
#define PASS(variant, nm, short, descrip) { short, &UocInfo::pass_##nm, descrip, false, false, variant },
#include "pass.def"
};

UocInfo::UocInfo(std::string _uocName, GCPtr<CoyImage> _ci)
{
  lastCompletedPass = pn_none;
  ast = 0;
  env = 0;
  ci = _ci;

  uocName = _uocName;

  path = resolveModulePath(uocName);

  if (!path)
    THROW(excpt::BadValue, 
	  format("File path for module \"%s\" not found", 
		 uocName.c_str()));
}

UocInfo::UocInfo(GCPtr<UocInfo> uoc)
{
  uocName = uoc->uocName;
  path = uoc->path;
  lastCompletedPass = uoc->lastCompletedPass;
  ast = uoc->ast;
  env = uoc->env;
  ci = uoc->ci;
}

/// @brief Given a module name, find the corresponding source file.
///
/// @bug This should be using the path concatenation code, not just
/// substituting '/'. The current implementation isn't compatible with
/// windows.
GCPtr<Path> 
UocInfo::resolveModulePath(std::string modName)
{
  if (modName == "coyotos.TargetInfo")
    modName = modName + "." + Options::targetArchName;

  std::string tmp = modName;
  for (size_t i = 0; i < tmp.size(); i++) {
    if (tmp[i] == '.')
      tmp[i] = '/';
  }
  Path ifPath(tmp);

  for (size_t i = 0; i < UocInfo::searchPath.size(); i++) {
    Path testPath = *UocInfo::searchPath[i] + ifPath << ".mki";
    
    if (testPath.exists() && testPath.isFile())
      return new Path(testPath);
  }

  return NULL;
}

sherpa::GCPtr< Environment<Value> > 
UocInfo::getExportedEnvironment()
{
  if (!exportedEnv) {
    exportedEnv = new Environment<Value>;

    for (size_t i = 0; i < env->bindings.size(); i++) {
      if (env->bindings[i]->flags & BF_EXPORTED)
	exportedEnv->bindings.append(env->bindings[i]);
    }
  }

  return exportedEnv;
}

#if 0
GCPtr<UocInfo> 
UocInfo::compileSource(const std::string& srcFileName,
		       GCPtr<CoyImage> ci)
{
  GCPtr<Path> path = new Path(srcFileName);

  GCPtr<UocInfo> puoci = new UocInfo(srcFileName, path, ci);

  puoci->Compile();

  UocInfo::uocList.append(puoci);
  return puoci;
}
#endif

GCPtr<UocInfo> 
UocInfo::importModule(std::ostream& errStream,
		      const LexLoc& loc, 
		      GCPtr<CoyImage> ci,
		      std::string modName,
		      uint32_t compileVariant)
{
  GCPtr<UocNodeType> nd = uocList.lookup(modName);

  importStack.push_back(ImportRecord(modName, loc));

  if (nd) {

    // Already have a record. Either the import is already completed
    // or it is currently in progress:

    if (nd->value) return nd->value;

    // A record exists without a unit of compilation pointer. This
    // means that the import is still in progress and we have a
    // circular import dependency:

    errStream << "Module \"" << modName << "\" has a circular import:\n";
    for (size_t i = 0; i < importStack.size() - 1; i++)
      errStream << "  Module " << importStack[i].nm 
		<< " imports " << importStack[i+1].nm
		<< " at " << importStack[i+1].loc <<	'\n';

    importStack.pop_back();

    THROW(excpt::BadValue, "Bad interpreter result");
  }

  uocList.insert(modName, GCPtr<UocInfo>(0));
  nd = uocList.lookup(modName);
   
  try {
    GCPtr<UocInfo> puoci = new UocInfo(modName, ci);
    puoci->Compile(compileVariant);

    nd->value = puoci;
  } catch ( const sherpa::UExceptionBase& ex ) {
    errStream
      << loc.asString() << ": "
      << "Error importing module \"" << modName << "\": "
      << ex.msg
      << endl;
    throw ex;
  }
  importStack.pop_back();

  return nd->value;
}


#if 0
GCPtr<UocInfo> 
UocInfo::findModule(const std::string& ifName)
{
  for (size_t i = 0; i < uocList.size(); i++) {
    if (uocList[i]->uocName == ifName)
      return uocList[i];
  }

  return NULL;
}

GCPtr<UocInfo> 
UocInfo::importInterface(std::ostream& errStream,
			 const LexLoc& loc, const std::string& ifName)
{
  // First check to see if we already have it. Yes, this
  // IS a gratuitously stupid way to do it.
  GCPtr<UocInfo> puoci = findInterface(ifName);

  if (puoci && puoci->ast)
    return puoci;

  if (puoci) {
    errStream
      << loc.asString() << ": "
      << "Fatal error: recursive dependency on interface "
      << "\"" << ifName << "\"\n"
      << "This needs a better diagnostic so you can decipher"
      << " what happened.\n";
    exit(1);
  }

  // Didn't find it. Actually need to do some work. Bother.
  puoci = new UocInfo(ifName, resolveInterfacePath(ifName));

  if (!puoci->path) {
    errStream
      << loc.asString() << ": "
      << "Import failed for interface \"" << ifName << "\".\n"
      << "Interface unit of compilation not found on search path.\n";
    exit(1);
  }

  // Need to append *first* so that we don't recurse infinitely 
  // while loading this interface:
  UocInfo::uocList.append(puoci);

  puoci->Compile();

  // The interface that we just compiled may have (recursively)
  // imported other interfaces. Now that we have successfully compiled
  // this interface, migrate it to the end of the interface list.

  for (size_t i = 0; i < UocInfo::uocList.size(); i++) {
    if (UocInfo::uocList[i] == puoci) {
      UocInfo::uocList.remove(i);
      UocInfo::uocList.append(puoci);
      break;
    }
  }

  return puoci;
}
#endif

bool
UocInfo::pass_none(std::ostream& errStream, unsigned long flags)
{
  return true;
}

bool
UocInfo::pass_npass(std::ostream& errStream, unsigned long flags)
{
  return true;
}


bool
UocInfo::pass_parse(std::ostream& errStream, unsigned long flags)
{
  // Use binary mode so that newline conversion and character set
  // conversion is not done by the stdio library.
  std::ifstream fin(path->c_str(), std::ios_base::binary);

  if (!fin.is_open()) {
    errStream << "Couldn't open input file \""
	      << *path
	      << "\"\n";
  }

  Lexer lexer(std::cerr, fin, path);

  // This is no longer necessary, because the parser now handles it
  // for all interfaces whose name starts with "bitc.xxx"
  //
  // if (this->flags & UOC_IS_PRELUDE)
  //   lexer.isRuntimeUoc = true;

  lexer.setDebug(Options::showLex);

  extern int mkiparse(Lexer *lexer, GCPtr<AST> &topAst);
  mkiparse(&lexer, ast);  
  // On exit, ast is a pointer to the AST tree root.
  
  fin.close();

  if (lexer.num_errors != 0u) {
    ast = new AST(at_Null);
    return false;
  }

  assert(ast->astType == at_uoc);

#if 0
  if (ast->child(0)->astType == at_interface && isSourceUoc) {
    errStream << ast->child(0)->loc.asString()
	      << ": Warning: interface units of compilation should "
	      << "no longer\n"
	      << "    be given on the command line.\n";
  }
#endif

  return true;
}

void
UocInfo::Compile(uint32_t compileVariant)
{
  for (size_t i = pn_none+1; (Pass)i <= pn_npass; i++) {
    if ((passInfo[i].variant & compileVariant) == 0)
      continue;
    
    if (Options::showPasses)
      std::cerr << uocName << " PASS " << passInfo[i].name << std::endl;

    //std::cout << "Now performing "
    //	      << passInfo[i].descrip
    //	      << " on " << path->asString()
    //	      << std::endl;

    if (! (this->*passInfo[i].fn)(std::cerr, 0) ) {
      std::cerr << "Exiting due to errors during "
		<< passInfo[i].descrip
		<< std::endl;
      exit(1);
    }

    if(!ast->isValid()) {
      std::cerr << "PANIC: Invalid AST built for file \""
		<< path->asString()
		<< "\". "
		<< "Please report this problem.\n"; 
      ast->PrettyPrint(std::cerr);
      std::cerr << std::endl;
      exit(1);
    }

    if (passInfo[i].printAfter) {
      std::cerr << "==== DUMPING "
		<< uocName
		<< " AFTER " << passInfo[i].name 
		<< " ====" << std::endl;
      this->PrettyPrint(std::cerr);
    }

    if (passInfo[i].stopAfter) {
      std::cerr << "Stopping (on request) after "
		<< passInfo[i].name
		<< std::endl;
      return;
    }

    lastCompletedPass = (Pass) i;
  }
}

void
UocInfo::PrettyPrint(std::ostream& out)
{
  ast->PrettyPrint(out);
  out << std::endl;
}
