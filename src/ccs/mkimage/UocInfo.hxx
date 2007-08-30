#ifndef UOCINFO_HXX
#define UOCINFO_HXX

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

#include <libsherpa/Path.hxx>
#include <libsherpa/LexLoc.hxx>
#include <libsherpa/CVector.hxx>
#include <libsherpa/avl.hxx>
#include <libcoyimage/CoyImage.hxx>
#include "Environment.hxx"
#include "AST.hxx"

// Passes consist of transforms on the AST. The parse pass is included in 
// the list for debugging annotation purposes, but is handled as a special
// case.

enum Pass {
#define PASS(variant, nm, short, descrip) pn_##nm,
#include "pass.def"
};

class UocInfo;

struct PassInfo {
  const char *name;
  bool (UocInfo::* fn)(std::ostream& errStream, unsigned long flags);
  const char *descrip;
  bool printAfter;		// for debugging
  bool stopAfter;		// for debugging
  uint32_t variant;
};

// Unit Of Compilation Info. One of these is constructed for each
// unit of compilation (source or interface). In the source case, the
// /name/ field will be the file name. In the interface case, the
// /name/ will be the "ifname" reported in the import statement.
class UocInfo : public Countable {
  static sherpa::GCPtr<sherpa::Path> resolveModulePath(std::string modName);

public:
  std::string uocName;		// either ifName or simulated
  GCPtr<sherpa::Path> path;
  unsigned long flags;
  
  bool isCmdLine;		// processing requested from command line

  Pass lastCompletedPass;

  sherpa::GCPtr<AST> ast;
  sherpa::GCPtr< Environment<Value> > env;
  sherpa::GCPtr< Environment<Value> > exportedEnv;

  sherpa::GCPtr< CoyImage > ci;
  
  UocInfo(std::string _uocName, GCPtr<CoyImage> ci);
  UocInfo(GCPtr<UocInfo> uoc);

  static sherpa::CVector< sherpa::GCPtr<sherpa::Path> > searchPath;

  struct ImportRecord {
    std::string nm;
    LexLoc loc;

    inline ImportRecord(const std::string& _nm, const LexLoc& _loc)
    {
      nm = _nm;
      loc = _loc;
    }
  };
  static std::vector<ImportRecord> importStack;

  // The presence of a UocInfo record in the uocList indicates that
  // parsing of a module has at least started. If the /ast/ pointer is
  // non-null, the parse has been completed.
  static AvlMap<std::string, GCPtr<UocInfo> > uocList;
  typedef AvlMap<std::string, GCPtr<UocInfo> >::NodeType UocNodeType;

  static GCPtr<UocInfo> 
  importModule(std::ostream&, 
	       const sherpa::LexLoc& loc, 
	       GCPtr<CoyImage> ci,
	       std::string modName,
	       uint32_t compileVariant);

  sherpa::GCPtr< Environment<Value> > getExportedEnvironment();

#if 0
  static GCPtr<UocInfo> 
  compileSource(const std::string& srcName, GCPtr<CoyImage> ci);
#endif

  // Individual passes:
#define PASS(variant, nm,short,descrip)			\
  bool pass_##nm(std::ostream& errStream, unsigned long flags=0);
#include "pass.def"

  static PassInfo passInfo[];  

  void Compile(uint32_t compileVariant);

  static GCPtr<AST> lookupByFqn(const std::string& s, GCPtr<UocInfo> &targetUoc);
  
  void PrettyPrint(std::ostream& out);

private:
};

#endif /* UOCINFO_HXX */
