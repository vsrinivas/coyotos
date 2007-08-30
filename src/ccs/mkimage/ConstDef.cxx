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

#include <assert.h>
#include <stddef.h>
#include <vector>
#include <string>
#include <iomanip>

#include <libsherpa/ifBinaryStream.hxx>
#include <libsherpa/ByteString.hxx>
#include <libcoyimage/ExecImage.hxx>

#include "Options.hxx"
#include "UocInfo.hxx"
#include "builtin.hxx"

// #define DEBUG

using namespace std;
using namespace sherpa;

bool
UocInfo::pass_defconst(std::ostream& errStream, unsigned long flags)
{
  //  if (!isCmdLine)
  //    return true;

#if 0
  /* This breaks for coyotos.TargetInfo, and in any case this is not
   * the appropriate place to perform this check. */
  GCPtr<AST> modName = ast->child(0);
  if (modName->s != uocName) {
    errStream << modName->loc << " "
	      << "Defined module name \"" << modName->s 
	      << "\" does not match file module name \"" << uocName << "\"."
	      << "\n";
    return false;
  }
#endif

  bool has_enums = false;

  for (size_t i = 1; i < ast->children->size(); i++) {
    GCPtr<AST> cast = ast->child(i);
    if (cast->astType == at_s_export_enum) {
      has_enums = true;
      break;
    }
  }

  if (!has_enums)
    return true;

  std::string fileName = uocName + ".h";
  if (Options::headerDir.size())
    fileName = (Path(Options::headerDir) + Path(fileName)).asString();;

#if 1
  ofstream ofs(fileName.c_str(), ofstream::out|ofstream::trunc);
  if (!ofs.is_open())
    THROW(excpt::IoError, 
	  format("Could not open \"%s\" for output.", fileName.c_str()));
#else
  ostream& ofs = std::cerr;
#endif

  for (size_t i = 1; i < ast->children->size(); i++) {
    GCPtr<AST> cast = ast->child(i);

    if (cast->astType != at_s_export_enum)
      continue;
    
    BigNum value(0);

    std::string dotEnumIdent = "";
    if (cast->child(0)->astType == at_ident)
      dotEnumIdent = "." + cast->child(0)->s;

    for (size_t nbind = 1; nbind < cast->children->size(); nbind++) {
      value = value + 1;

      GCPtr<AST> binding = cast->child(nbind);
      std::string ident= binding->child(0)->s;

      // FIX: This is completely broken for unicode!
      std::string fqn = uocName + dotEnumIdent + "." + ident;
      for (size_t i = 0; i < fqn.size(); i++) {
	if (fqn[i] == '.' || fqn[i] == '-')
	  fqn[i] = '_';
      }
      
      // We know it is an integer value because the interpreter
      // required this:
      if ( binding->children->size() == 2) {
	GCPtr<IntValue> v = binding->child(1)->litValue.upcast<IntValue>();
	value = v->bn;
      }

      ofs << "#define " << fqn << " " << value << '\n';
    }
  }

  return true;
}
