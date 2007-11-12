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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include <string>
#include <ostream>
#include <fstream>

#define REGISTER_ALIGN_BY_HOLE

/* Size of largest integral type that we will attempt to registerize: */
#define MAX_REGISTERIZABLE 64

#define FIRST_IN_DATA_REG   2   /* reserve 0 for syscall word, 1 for opcode/result code */
#define FIRST_OUT_DATA_REG  1	/* reserve 0 for syscall word */
#define MAX_DATA_REG     8

#define FIRST_CAP_REG 0		/* 0 used for reply cap on send */
#define MAX_CAP_REG   4

/** @brief Control marshalling alignment for long long.
 *
 * On some architectures, notably SPARC32, alignof(long long) is
 * 2*alignof(long). On these machines, we have four options:
 *
 *  1. Exclude any type whose alignment exceeds word size from
 *     registerization. This is unfortunate, because 
 *
 *  2. When this case arises during registerization, we can skip
 *     a register, leaving it unused.
 *
 *  3. When this case arises during registerization, we can rearrange
 *     the argument vector in the hopes that we will not need to skip
 *
 *  4. We can ship LONG LONGs at improper alignments and demarshal
 *     them.
 *
 * Option (1) has to be ruled out, because it pushes the kernel GPT
 * interface and several others into a string on such platforms.
 *
 * Option (2) is viable, but (e.g.) the makeLocalWindow() method as
 * originally specified would not fit in registers without rearranging
 * some arguments to accomodate alignment issues. This creates an
 * opportunity for error in the course of future kernel changes, where
 * we inadvertently introduce another non-registerizable case.  Worse,
 * it introduces platform-dependent in addition to word-size dependent
 * demarshalling.
 *
 *  This is the option that I am test-implementing at the moment.
 *
 * Option (3) is sensible, but it is more complicated than I really
 * have time to implement right now.
 *
 * Option (4) Creates problems for server side (de)marshal, because it
 * makes it impossible to create an overlay structure on the register
 * words that can be sensibly passed to the handler procedure. We
 * would like to avoid that.
 */

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#define COYOTOS_UNIVERSAL_CROSS
#include <coyotos/machine/target.h>

#include <libsherpa/Path.hxx>
#include "INOstream.hxx"
#include "SymTab.hxx"
#include "ArchInfo.hxx"
#include "backend.hxx"
#include "capidl.hxx"
#include "util.hxx"

static void print_asmifdef(INOstream& out);
static void print_asmendif(INOstream& out);

/* c_byreftype is an optimization -- currently disabled, and needs
   debugging */
static bool
c_byreftype(GCPtr<Symbol> s)
{
  return false;
#if 0
  switch(s->cls) {
  case sc_symRef:
    {
      return c_byreftype(s->value);
    }
  case sc_struct:
  case sc_union:
  case sc_seqType:		// Maybe
  case sc_arrayType:
    {
      return true;
    }


  default:
    return false;
  }
#endif
}

static void
output_c_type_trailer(GCPtr<Symbol> s, INOstream& out)
{
  s = s->ResolveRef();

  if (s->cls == sc_typedef)
    return;

#if 0
  /* FIX: This seems wrong to me. If it is truly a variable length
   * sequence type then this bound computation may not be statically
   * feasible. */
  if (symbol_IsVarSequenceType(s) || symbol_IsFixSequenceType(s)) {
    BigNum bound = compute_value(s->value);

    // The bound on the size
    fprintf(out, "[%u]", mpz_get_ui(&bound));
  }
#endif
  if (s->IsFixSequenceType()) {
    BigNum bound = compute_value(s->value);

    // The bound on the size
    out << "[" << bound << "]";
  }
}

static void
output_c_type(GCPtr<Symbol> s, INOstream& out)
{
  s = s->ResolveRef();

  /* If this symbol is a typedef, we want to put out the typedef name
     as its type name. Since the subsequent type classification checks
     will bypass typedefs to expose the underlying type, handle
     typedefs here. */

  if (s->cls == sc_typedef) {
    out << s->QualifiedName('_');
    return;
  }

  s = s->ResolveType();

  if (s->IsSequenceType()) {
    out << "struct {\n";
    out.more();
    out << "unsigned long max;\n";
    out << "unsigned long len;\n";
    output_c_type(s->type, out);
    out << " *data;\n";
    out.less();
    out << "}";

    return;
  }

  if (s->IsFixSequenceType()) {
    // We are doing the LHS of the type (the part that appears before
    // the identifier). Simply recurse into the array element type.
    output_c_type(s->type, out);
    return;
  }

  while (s->cls == sc_symRef)
    s = s->value;

  switch(s->cls) {
  case sc_primtype:
    {
      switch(s->v.lty){
      case lt_unsigned:
	switch(s->v.bn.as_uint32()) {
	case 0:
	  out << "Integer";
	  break;
	case 8:
	  out << "uint8_t";
	  break;
	case 16:
	  out << "uint16_t";
	  break;
	case 32:
	  out << "uint32_t";
	  break;
	case 64:
	  out << "uint64_t";
	  break;
	default:
	  out << "/* unknown integer size */";
	  break;
	};

	break;
      case lt_integer:
	switch(s->v.bn.as_uint32()) {
	case 0:
	  out << "Integer";
	  break;
	case 8:
	  out << "int8_t";
	  break;
	case 16:
	  out << "int16_t";
	  break;
	case 32:
	  out << "int32_t";
	  break;
	case 64:
	  out << "int64_t";
	  break;
	default:
	  out << "/* unknown integer size */";
	  break;
	};

	break;
      case lt_char:
	switch(s->v.bn.as_uint32()) {
	case 0:
	case 32:
	  out << "wchar_t";
	  break;
	case 8:
	  out << "char";
	  break;
	default:
	  out << "/* unknown wchar size */";
	  break;
	}
	break;
      case lt_string:
	switch(s->v.bn.as_uint32()) {
	case 0:
	case 32:
	  out << "wchar_t *";
	  break;
	case 8:
	  out << "char *";
	  break;
	default:
	  out << "/* unknown string size */";
	  break;
	}
	break;
      case lt_float:
	switch(s->v.bn.as_uint32()) {
	case 32:
	  out << "float";
	  break;
	case 64:
	  out << "double";
	  break;
	case 128:
	  out << "long double";
	  break;
	default:
	  out << "/* unknown float size */";
	  break;
	}
	break;
      case lt_bool:
	{
	  out << "bool";
	  break;
	}
      case lt_void:
	{
	  out << "void";
	  break;
	}
      default:
	out << "/* Bad primtype type code */";
	break;
      }
      break;
    }
  case sc_symRef:
  case sc_typedef:
    {
      /* Should have been caught by the symbol resolution calls above */
      assert(false);
      break;
    }
  case sc_interface:
  case sc_absinterface:
    {
      out << "caploc_t";
      break;
    }
  case sc_struct:
  case sc_union:
    {
      out << s->QualifiedName('_');
      break;
    }

  case sc_seqType:
    {
      out << "/* some varSeqType */";
      break;
    }

  case sc_arrayType:
    {
      out << "/* some fixSeqType */";
      break;
    }

  default:
    out << s->QualifiedName('_');
    break;
  }
}

typedef enum {
  mc_reg,
  mc_cap,
  mc_string
} ArgClass;

typedef struct {
  SymVec regs;
  SymVec caps;
  SymVec strings;
  size_t nReg;
  bool   needStruct;
  size_t regIndirectBytes;
  size_t indirectBytes;
  size_t indirectWords;
} UniParams;			// unidirectional

typedef sherpa::CVector< ArgClass > ArgClassVec;

typedef struct ArgInfo {
  UniParams in;
  UniParams out;

  /* This vector is positionally correlated with the child vector to
     indicate how each child was marshalled. If the return type is
     non-void, there may be an additional entry at the end for the
     return type. */
  ArgClassVec  marshallClass;
} ArgInfo;

static void
extract_args_compute_indirect_bytes(UniParams& args)
{
  for (size_t i = 0; i < args.regs.size(); i++) {
    GCPtr<Symbol> child = args.regs[i];
    args.regIndirectBytes = round_up(args.regIndirectBytes, targetArch->wordBytes);
    args.regIndirectBytes += child->type->indirectSize(*targetArch);
  }

  args.indirectBytes = args.regIndirectBytes;
  for (size_t i = 0; i < args.strings.size(); i++) {
    GCPtr<Symbol> child = args.strings[i];
    args.indirectBytes = round_up(args.indirectBytes, targetArch->wordBytes);
    args.indirectBytes += child->type->indirectSize(*targetArch);
  }

  args.indirectWords = round_up(args.indirectBytes, targetArch->wordBytes);
  args.indirectWords /= targetArch->wordBytes;
}

static void
extract_args(GCPtr<Symbol> s, ArgInfo& args)
{
  args.in.nReg = FIRST_IN_DATA_REG;
  args.out.nReg = FIRST_OUT_DATA_REG;
  args.in.needStruct = false;	// until proven otherwise
  args.out.needStruct = false;	// until proven otherwise
  args.in.regIndirectBytes = 0;
  args.in.indirectBytes = 0;
  args.out.regIndirectBytes = 0;
  args.out.indirectBytes = 0;

  /// @bug There is a severe problem here if the return value is of
  /// capability type: where to return it, since this is actually a
  /// by-reference type.

  for(size_t i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> child = s->children[i];

    UniParams& param = (child->cls == sc_formal) ? args.in : args.out;

    if (child->type->IsInterface()) {
      param.caps.append(child);
      args.marshallClass.append(mc_cap);
      continue;
    }

    size_t align = child->alignof(*targetArch);

    size_t db = child->directSize(*targetArch);
    size_t dr = round_up(db, targetArch->wordBytes);
    dr /= targetArch->wordBytes;


    assert (align <= 2 * targetArch->wordBytes);

#ifdef REGISTER_ALIGN_BY_HOLE
    {
      size_t curAlign = param.nReg * targetArch->wordBytes;
      assert (align <= (2 * targetArch->wordBytes));

      size_t nHole = 0;
      while (curAlign < align) {
	nHole++;
	curAlign = (param.nReg + nHole) * targetArch->wordBytes;
      }

      dr += nHole;
    }
#endif

    // We do not consider further registerizations once we start a
    // string. This may be a mistake.
    if ((param.strings.size() == 0) && (param.nReg + dr <= MAX_DATA_REG)) {
      param.regs.append(child);
      args.marshallClass.append(mc_reg);
      param.nReg += dr;
    }
    else {
      param.strings.append(child);
      args.marshallClass.append(mc_string);
    }
  }

  if (!s->type->IsVoidType()) {
    UniParams& param = args.out;

    // NOTE: This symbol is not in any scope, because I do not want
    // to mung the procedure scope. However, it's TYPE is in a
    // scope, which is what must be true for the rest of the backend
    // code to work correctly.
    GCPtr<Symbol> retVal = 
      Symbol::Create_inScope(LToken(s->loc, "_retVal"), s->isActiveUOC, 
			     sc_outformal, GCPtr<Symbol>(0));
    retVal->type = s->type->ResolveType();

    if (retVal->type->IsInterface()) {
      param.caps.append(retVal);
      args.marshallClass.append(mc_cap);
    }
    else {
      size_t align = retVal->type->alignof(*targetArch);

      size_t db = retVal->directSize(*targetArch);
      size_t dr = round_up(db, targetArch->wordBytes);
      dr /= targetArch->wordBytes;

#ifdef REGISTER_ALIGN_BY_HOLE
      {
	size_t curAlign = (param.nReg) * targetArch->wordBytes;
	assert (align <= (2 * targetArch->wordBytes));

	size_t nHole = 0;
	while (curAlign < align) {
	  nHole++;
	  curAlign = (param.nReg + nHole) * targetArch->wordBytes;
	}

	dr += nHole;
      }
#endif

      // We do not consider further registerizations once we start a
      // string. This may be a mistake.
      if ((param.strings.size() == 0) && (param.nReg + dr <= MAX_DATA_REG)) {
	param.regs.append(retVal);
	args.marshallClass.append(mc_reg);
	param.nReg += dr;
      }
      else {
	param.strings.append(retVal);
	args.marshallClass.append(mc_string);
      }
    }
  }

  if (args.in.caps.size() > MAX_CAP_REG) {
    fprintf(stderr, "Too many capability arguments to method %s\n",
	    s->QualifiedName('.').c_str());
    exit(1);
  }
  if (args.out.caps.size() > MAX_CAP_REG) {
    fprintf(stderr, "Too many capability results from method %s\n",
	    s->QualifiedName('.').c_str());
    exit(1);
  }

  extract_args_compute_indirect_bytes(args.in);
  extract_args_compute_indirect_bytes(args.out);

  if (args.in.strings.size() || args.in.indirectBytes)
    args.in.needStruct = true;

  if (args.out.strings.size() || args.out.indirectBytes)
    args.out.needStruct = true;
}

static void
emit_marshall_decl(GCPtr<Symbol> s, INOstream& out, ArgInfo& args)
{
  /* FOLLOWING IS THE EXPERIMENTAL NEW STYLE DECLARATION */
  out << "typedef union {\n";
  out.more();

  /* Emit the client input argument structure. */
  {
    out << "struct {\n";
    out.more();

    /* Parameter words */
    out << "uintptr_t  _icw;   /* _pw0 */\n";
    out << "uintptr_t  _opCode; /*_pw1 */\n";

    size_t regArgWords = FIRST_IN_DATA_REG;
    for (size_t i = 0; i < args.in.regs.size(); i++) {
      GCPtr<Symbol> child = args.in.regs[i];
      size_t align = child->alignof(*targetArch);
      size_t db = child->directSize(*targetArch);
      size_t dr = round_up(db, targetArch->wordBytes);
      dr /= targetArch->wordBytes;

      assert(dr > 0);

#ifdef REGISTER_ALIGN_BY_HOLE
      {
	size_t curAlign = regArgWords * targetArch->wordBytes;
	assert (align <= (2 * targetArch->wordBytes));

	while (curAlign < align) {
	  out << "uintptr_t  _pw" << regArgWords << ";\n";

	  regArgWords++;
	  curAlign = regArgWords * targetArch->wordBytes;
	}
      }
#endif

      if (dr == 1) {
	output_c_type(child->type, out);
	out << "  " << child->name;
	output_c_type_trailer(child->type, out);
	out << "; /* _pw" << regArgWords << " */\n";
	
	if (db < targetArch->wordBytes)
	  out << "uintptr_t  :0; /* pad to word */\n";
      }
      else {
	output_c_type(child->type, out);
	out << "  " << child->name;
	output_c_type_trailer(child->type, out);
	out << "; /* _pw" << regArgWords 
	    << ".._pw" << regArgWords + (dr-1) << " */\n";
      }

      regArgWords += dr;
    }
    for (; regArgWords < MAX_DATA_REG; regArgWords++)
      out << "uintptr_t  _pw" << regArgWords << ";\n";


    out << "caploc_t   _invCap;\n";

    /* Send caps */
    out << "caploc_t   _replyCap; /* _sndCap0 */\n";
    {
      for (size_t i = 1; i < MAX_CAP_REG; i++) {
	if (i <= args.in.caps.size()) {
	  GCPtr<Symbol> child = args.in.caps[i-1];

	  out << "caploc_t   " <<  child->name 
	      << "; /* _sndCap" << i << " */\n";
	}
	else
	  out << "caploc_t   _sndCap" <<  i << ";\n";
      }
    }
    /* Receive caps */
    for (size_t i = 0; i < MAX_CAP_REG; i++) 
      out << "caploc_t   _rcvCap" <<  i << ";\n";

    out << "uint32_t   _sndLen;\n";	// FIX: Should be 16 bits
    out << "uint32_t   _rcvBound;\n";	// FIX: Should be 16 bits
    out << "void      *_sndPtr;\n";
    out << "void      *_rcvPtr;\n";
    out << "uint64_t   _epID;\n";

    if (args.in.strings.size()) {
      for (size_t i = 0; i < args.in.strings.size(); i++) {
	GCPtr<Symbol> child = args.in.strings[i];
	output_c_type(child->type, out);
	out << " ";
	out << child->name;
	output_c_type_trailer(child->type, out);
	out << ";\n";
      }
    }

    if (args.in.indirectBytes)
      out << "char _indirect[" << args.in.indirectBytes << "];\n";

    out.less();
    out << "} in;\n";
  }

  /* Emit the server request input argument structure. Regrettably
   * this is not quite the same as the client version, because the
   * capability register locations change. */
  {
    out << "struct {\n";
    out.more();

    /* Parameter words */
    out << "uintptr_t  _icw;   /* _pw0 */\n";
    out << "uintptr_t  _opCode; /*_pw1 */\n";

    size_t regArgWords = FIRST_IN_DATA_REG;
    for (size_t i = 0; i < args.in.regs.size(); i++) {
      GCPtr<Symbol> child = args.in.regs[i];
      size_t align = child->alignof(*targetArch);
      size_t db = child->directSize(*targetArch);
      size_t dr = round_up(db, targetArch->wordBytes);
      dr /= targetArch->wordBytes;

      assert(dr > 0);

#ifdef REGISTER_ALIGN_BY_HOLE
      {
	size_t curAlign = regArgWords * targetArch->wordBytes;
	assert (align <= (2 * targetArch->wordBytes));

	while (curAlign < align) {
	  out << "uintptr_t  _pw" << regArgWords << ";\n";

	  regArgWords++;
	  curAlign = regArgWords * targetArch->wordBytes;
	}
      }
#endif

      if (dr == 1) {
	output_c_type(child->type, out);
	out << "  " << child->name;
	output_c_type_trailer(child->type, out);
	out << "; /* _pw" << regArgWords << " */\n";
	
	if (db < targetArch->wordBytes)
	  out << "uintptr_t  :0; /* pad to word */\n";
      }
      else {
	output_c_type(child->type, out);
	out << "  " << child->name;
	output_c_type_trailer(child->type, out);
	out << "; /* _pw" << regArgWords 
	    << ".._pw" << regArgWords + (dr-1) << " */\n";
      }

      regArgWords += dr;
    }
    for (; regArgWords < MAX_DATA_REG; regArgWords++)
      out << "uintptr_t  _pw" << regArgWords << ";\n";


    out << "uintptr_t  _pp;\n";

    /* Send caps */
    for (size_t i = 0; i < MAX_CAP_REG; i++) 
      out << "caploc_t   _sndCap" <<  i << ";\n";

    /* Receive caps */
    out << "caploc_t   _replyCap; /* _sndCap0 */\n";
    {
      for (size_t i = 1; i < MAX_CAP_REG; i++) {
	if (i <= args.in.caps.size()) {
	  GCPtr<Symbol> child = args.in.caps[i-1];

	  out << "caploc_t   " <<  child->name 
	      << "; /* _rcvCap" << i << " */\n";
	}
	else
	  out << "caploc_t   _rcvCap" <<  i << ";\n";
      }
    }

    out << "uint32_t   _sndLen;\n";	// FIX: Should be 16 bits
    out << "uint32_t   _rcvBound;\n";	// FIX: Should be 16 bits
    out << "void      *_sndPtr;\n";
    out << "void      *_rcvPtr;\n";
    out << "uint64_t   _epID;\n";

    if (args.in.strings.size()) {
      for (size_t i = 0; i < args.in.strings.size(); i++) {
	GCPtr<Symbol> child = args.in.strings[i];
	output_c_type(child->type, out);
	out << " ";
	out << child->name;
	output_c_type_trailer(child->type, out);
	out << ";\n";
      }
    }

    if (args.in.indirectBytes)
      out << "char _indirect[" << args.in.indirectBytes << "];\n";

    out.less();
    out << "} server_in;\n";
  }

  /* Emit the client output argument structure. */
  if (s->cls != sc_oneway) {
    out << "struct {\n";
    out.more();

    out << "uintptr_t  _icw;   /* _pw0 */\n";

    size_t regArgWords = FIRST_OUT_DATA_REG;

    for (size_t i = 0; i < args.out.regs.size(); i++) {
      GCPtr<Symbol> child = args.out.regs[i];
      size_t align = child->alignof(*targetArch);
      size_t db = child->directSize(*targetArch);
      size_t dr = round_up(db, targetArch->wordBytes);
      dr /= targetArch->wordBytes;

      assert(dr > 0);

#ifdef REGISTER_ALIGN_BY_HOLE
      {
	size_t curAlign = regArgWords * targetArch->wordBytes;
	assert (align <= (2 * targetArch->wordBytes));

	while (curAlign < align) {
	  out << "uintptr_t  _pw" << regArgWords << ";\n";

	  regArgWords++;
	  curAlign = regArgWords * targetArch->wordBytes;
	}
      }
#endif

      if (dr == 1) {
	output_c_type(child->type, out);
	out << "  " << child->name;
	output_c_type_trailer(child->type, out);
	out << "; /* _pw" << regArgWords << " */\n";
	
	if (db < targetArch->wordBytes)
	  out << "uintptr_t  :0; /* pad to word */\n";
      }
      else {
	output_c_type(child->type, out);
	out << "  " << child->name;
	output_c_type_trailer(child->type, out);
	out << "; /* _pw" << regArgWords 
	    << ".._pw" << regArgWords + (dr-1) << " */\n";
      }

      regArgWords += dr;
    }
    for (; regArgWords < MAX_DATA_REG; regArgWords++)
      out << "uintptr_t  _pw" << regArgWords << ";\n";

    out << "uintptr_t  _pp;\n";

    /* Send caps */
    for (size_t i = 0; i < MAX_CAP_REG; i++) 
      out << "caploc_t   _sndCap" <<  i << ";\n";
    /* Receive caps */
    {
      for (size_t i = FIRST_CAP_REG; i < MAX_CAP_REG; i++) {
	if (i < args.out.caps.size()) {
	  GCPtr<Symbol> child = args.out.caps[i];

	  out << "caploc_t   " <<  child->name 
	      << "; /* _rcvCap" << i << " */\n";
	}
	else
	  out << "caploc_t   _rcvCap" <<  i << ";\n";
      }
    }

    out << "uint32_t   _sndLen;\n";	// FIX: Should be 16 bits
    out << "uint32_t   _rcvBound;\n";	// FIX: Should be 16 bits
    out << "void      *_sndPtr;\n";
    out << "void      *_rcvPtr;\n";
    out << "uint64_t   _epID;\n";

    if (args.out.strings.size()) {
      for (size_t i = 0; i < args.out.strings.size(); i++) {
	GCPtr<Symbol> child = args.out.strings[i];
	output_c_type(child->type, out);
	out << " ";
	out << child->name;
	output_c_type_trailer(child->type, out);
	out << ";\n";
      }
    }

    if (args.out.indirectBytes)
      out << "char _indirect[" << args.out.indirectBytes << "];\n";

    out.less();
    out << "} out;\n";
  }

  /* Emit the server output argument structure. */
  if (s->cls != sc_oneway) {
    out << "struct {\n";
    out.more();

    out << "uintptr_t  _icw;   /* _pw0 */\n";

    size_t regArgWords = FIRST_OUT_DATA_REG;

    for (size_t i = 0; i < args.out.regs.size(); i++) {
      GCPtr<Symbol> child = args.out.regs[i];
      size_t align = child->alignof(*targetArch);
      size_t db = child->directSize(*targetArch);
      size_t dr = round_up(db, targetArch->wordBytes);
      dr /= targetArch->wordBytes;

      assert(dr > 0);

#ifdef REGISTER_ALIGN_BY_HOLE
      {
	size_t curAlign = regArgWords * targetArch->wordBytes;
	assert (align <= (2 * targetArch->wordBytes));

	while (curAlign < align) {
	  out << "uintptr_t  _pw" << regArgWords << ";\n";

	  regArgWords++;
	  curAlign = regArgWords * targetArch->wordBytes;
	}
      }
#endif

      if (dr == 1) {
	output_c_type(child->type, out);
	out << "  " << child->name;
	output_c_type_trailer(child->type, out);
	out << "; /* _pw" << regArgWords << " */\n";
	
	if (db < targetArch->wordBytes)
	  out << "uintptr_t  :0; /* pad to word */\n";
      }
      else {
	output_c_type(child->type, out);
	out << "  " << child->name;
	output_c_type_trailer(child->type, out);
	out << "; /* _pw" << regArgWords 
	    << ".._pw" << regArgWords + (dr-1) << " */\n";
      }

      regArgWords += dr;
    }
    for (; regArgWords < MAX_DATA_REG; regArgWords++)
      out << "uintptr_t  _pw" << regArgWords << ";\n";

    out << "caploc_t   _invCap;\n";

    /* Send caps */
    {
      for (size_t i = FIRST_CAP_REG; i < MAX_CAP_REG; i++) {
	if (i < args.out.caps.size()) {
	  GCPtr<Symbol> child = args.out.caps[i];

	  out << "caploc_t   " <<  child->name 
	      << "; /* _sndCap" << i << " */\n";
	}
	else
	  out << "caploc_t   _sndCap" <<  i << ";\n";
      }
    }
    /* Receive caps */
    for (size_t i = 0; i < MAX_CAP_REG; i++) 
      out << "caploc_t   _rcvCap" <<  i << ";\n";

    out << "uint32_t   _sndLen;\n";	// FIX: Should be 16 bits
    out << "uint32_t   _rcvBound;\n";	// FIX: Should be 16 bits
    out << "void      *_sndPtr;\n";
    out << "void      *_rcvPtr;\n";
    out << "uint64_t   _epID;\n";

    if (args.out.strings.size()) {
      for (size_t i = 0; i < args.out.strings.size(); i++) {
	GCPtr<Symbol> child = args.out.strings[i];
	output_c_type(child->type, out);
	out << " ";
	out << child->name;
	output_c_type_trailer(child->type, out);
	out << ";\n";
      }
    }

    if (args.out.indirectBytes)
      out << "char _indirect[" << args.out.indirectBytes << "];\n";

    out.less();
    out << "} server_out;\n";
  }

  /* Emit the common access structure */
  {
    out << "InvParameterBlock_t pb;\n";
    out << "InvExceptionParameterBlock_t except;\n";
  }

  out.less();
  out << "} _INV_" << s->QualifiedName('_') << ";\n";
  out << "\n";
}

static void
emit_in_param(GCPtr<Symbol> s, INOstream& out)
{
  GCPtr<Symbol> argBaseType = s->type->ResolveType();
  if (argBaseType->IsFixSequenceType()) {
    BigNum bound = compute_value(argBaseType->value);
    out << "__builtin_memcpy(&_params.in."
	<< s->name
	<< ", "
	<< s->name
	<< ", sizeof("
	<< s->name
	<< "[0]) * " << bound << ");\n";
  }
  else {
    out << "_params.in."
	<< s->name
	<< " = "
	<< s->name
	<< ";\n";
  }

  if (argBaseType->IsSequenceType()) {
    /* Do not disclose source pointer address. This probably is
       excessively paranoid. It will be clobbered on the receive
       side anyway. Leaking it probably doesn't hurt much, but why
       take chances? */
    out << "_params.in."
	<< s->name
	<< ".data = 0;\n";
    out << "/* Indirect Component: */\n";
    out << "{\n";

    out.more();
    out << "size_t _nBytes = " << s->name << ".len * "
	<< "sizeof(" << s->name << ".data[0]);\n"
	<< "_nBytes = IDL_ALIGN_TO(_nBytes, sizeof(uintptr_t));\n"
#if 0
	<< "size_t _nWords = _nBytes/sizeof(uintptr_t);\n"
	<< "/* Avoid trailing data leak */\n"
	<< "params.in._indirect[_params.in._sndLen + _nWords - 1] = 0;\n"
#endif
	<< "\n"
	<< "__builtin_memcpy(&_params.in._indirect[_params.in._sndLen], "
	<< s->name
	<< ".data, _nBytes);\n"
	<< "_params.in._sndLen += _nBytes;\n"; // FIX
    out.less();

    out << "}\n";
  }
}

static void
emit_in_marshall(GCPtr<Symbol> s, INOstream& out, UniParams& args)
{
  if (args.strings.size() && args.indirectBytes)
    out << "_params.in._sndLen = (offsetof(typeof(_params), in._indirect) - "
	<< "offsetof(typeof(_params), in."
	<< args.strings[0]->name << "));\n";
  else if (args.strings.size()) 
    out << "_params.in._sndLen = (sizeof(_params.in) - "
	<< "offsetof(typeof(_params), in."
	<< args.strings[0]->name << "));\n";
  else
    out << "_params.in._sndLen = 0;\n";
  out << "\n";

  out << "_params.in._opCode = OC_"
      << s->QualifiedName('_') << ";\n";

  out << "_params.in._invCap = _invCap;\n";
  if (s->cls == sc_oneway) {
    out << "_params.in._replyCap = REG_CAPLOC(0);\n";
    out << "_params.in._epID = _env->epID;\n";
  }
  else {
    out << "_params.in._replyCap = _env->replyCap;\n";
    out << "_params.in._epID = _env->epID;\n";
  }

  for (size_t i = 0; i < args.regs.size(); i++)
    emit_in_param(args.regs[i], out);

  for (size_t i = 0; i < args.caps.size(); i++)
    emit_in_param(args.caps[i], out);

  for (size_t i = 0; i < args.strings.size(); i++)
    emit_in_param(args.strings[i], out);



  if (args.strings.size())
    out << "_params.in._sndPtr = &_params.in."
	<< args.strings[0]->name << ";\n";
  else if (args.indirectBytes)
    out << "_params.in._sndPtr = &_params.in._indirect;\n";
  out << "\n";
}

static void
emit_out_param(GCPtr<Symbol> s, INOstream& out)
{
  assert(s->type->ResolveType()->IsInterface());

  out << "_params.out."
      << s->name
      << " = "
      << s->name
      << ";\n";
}

/** @bug There is an error here that register out parameters go to
 * their destinations prior to the check of the _errCode value. This
 * eliminates an extra copy at the cost of making the output state of
 * these variables undefined. For C this is probably acceptable. For
 * safe programming languages it is certainly @em not acceptable.
 *
 * Addendum, 7/26/2007: This may now be resolved?
 */
static void
emit_out_marshall(GCPtr<Symbol> s, INOstream& out, UniParams& args)
{
  if (s->cls == sc_oneway)
    return;

  for (size_t i = 0; i < args.caps.size(); i++)
    emit_out_param(args.caps[i], out);

  if (args.strings.size()) {
    out << "_params.out._rcvBound = sizeof(_params.out) - "
	<< "offsetof(__typeof__(_params.out), "
	<< args.strings[0]->name
	<< ");\n";
    out << "_params.out._rcvPtr = &_params.out."
	<< args.strings[0]->name
	<< ";\n";
  }
  else if (args.indirectBytes) {
    out << "_params.out._rcvBound = sizeof(_params.out) - " 
	<< "offsetof(__typeof__(_params.out)__, _indirect);\n";
    out << "_params.out._rcvPtr = &params.out.indirect;\n"
	<< ";\n";
  }
  else
    out << "_params.out._rcvBound = 0;\n";
  out << "\n";

  if (args.strings.size() || args.indirectBytes) {
    out << "{\n";
    out.more();
    out << "size_t _IDL_i;\n"
	<< "/* Probe incoming string region on stack to ensure validity */\n"
	<< "for (_IDL_i = 0; _IDL_i < _params.out._rcvBound; _IDL_i+= COYOTOS_PAGE_SIZE)\n";
    {
      out.more();
      out << "((char *)_params.out._rcvPtr)[_IDL_i] &= 0xff;\n";
      out.less();
    }
    out << "((char *)_params.out._rcvPtr)[_params.out._rcvBound - 1] &= 0xff;\n";
    out.less();
    out << "}\n";
  }
  out << "\n";
}

static void
emit_out_demarshall_result(GCPtr<Symbol> s, INOstream& out)
{
  if (s->cls == sc_oneway)
    return;

  GCPtr<Symbol> argBaseType = s->type->ResolveType();

  if (argBaseType->IsFixSequenceType()) {
    BigNum bound = compute_value(argBaseType->value);
    out << "__builtin_memcpy("
	<< s->name
	<< ", &_params->out."
	<< s->name
	<< ", "
	<< s->name
	<< ", sizeof("
	<< s->name
	<< "[0]) * " << bound << ");\n";
  }
  else {
    out << "*"
	<< s->name
	<< " = _params.out."
	<< s->name
	<< ";\n";
  }

  if (argBaseType->IsSequenceType()) {
    BigNum bound = compute_value(argBaseType->value);

    /* Do not disclose source pointer address. This probably is
       excessively paranoid. It will be clobbered on the receive
       side anyway. Leaking it probably doesn't hurt much, but why
       take chances? */
    out << "/* Indirect Component: */\n";
    out << "{\n";
    out.more();
    out << "/* Caller responsibility to allocate indirect rcv buffer */\n"
	<< "__builtin_memcpy("
	<< s->name << "->data, "
	<< " &_params.out._indirect[_outIndirNdx], "
	<< s->name << "->len);\n";
    out << s->name
	<< "->max = " << bound << ";\n";
    out << "size_t _nBytes = sizeof("
	<< s->name << "->data[0]) * "
	<< s->name << "->len;\n"
	<< "_nBytes = IDL_ALIGN_TO(_nBytes, sizeof(uintptr_t));\n"
	<< "_outIndirNdx += _nBytes;\n";
    out.less();
    out << "}\n";
  }
}


static void
emit_out_demarshall(GCPtr<Symbol> s, INOstream& out, 
		    UniParams& args, size_t nHardRegs)
{
  if (s->cls == sc_oneway)
    return;

  if (! (args.regs.size() || args.strings.size()) )
    return;

  if (args.indirectBytes) {
    out << "size_t _outIndirNdx = 0;\n";
    out << "\n";
  }

  for (size_t i = 0; i < args.regs.size(); i++)
    emit_out_demarshall_result(args.regs[i], out);

  for (size_t i = 0; i < args.strings.size(); i++)
    emit_out_demarshall_result(args.strings[i], out);
}

#if 0
static void
emit_i386_syscall(GCPtr<Symbol> s, INOstream& out, ArgInfo& args)
{
  /* Determine how big a stack structure we need for parameter
   * words. This could probably be reworked to get a smaller stack
   * frame in many cases, but for the moment I am just going to do
   * the easy thing here. Since slots 0..3 are unused, we will
   * save true ESP to pw[0].
   *
   * Not clear (to me) if auto initialization of this form is
   * allowed for non-GCC. If not, it can be revised to initialize
   * the painful way.
   */
  out << "/* Emit syscall: */\n";
  out << "struct {\n";
  out.more();
  out << "uintptr_t pw[23];\n";
  out.less();
  out << "} _softPW = {\n";
  out.more();

  out << ".pw[IPW_INVCAP] = _invCap";

  for (size_t pw = 4; pw < args.in.nReg; pw++)
    out << ",\n.pw[" << pw << "] = _pw" << pw;

  out << ",\n.pw[IPW_SNDCAP+0] = _env->replyCap";
  
  for (size_t i = FIRST_CAP_REG+1; i <= args.in.caps.size(); i++)
    out << ",\n.pw[IPW_SNDCAP+" << i << "] = "
	<< args.in.caps[i-1]->name;

  for (size_t i = FIRST_CAP_REG; i < MAX_CAP_REG; i++)
    if (args.out.caps.size() > i)
      out << ",\n.pw[IPW_RCVCAP+" << i << "] = "
	  << args.out.caps[i]->name;

  out << ",\n.pw[IPW_RCVBOUND] = _outStrLen";
  if (args.in.needStruct)
    out << ",\n.pw[IPW_RCVPTR] = (uintptr_t) &_in";
  out << ",\n.pw[IPW_SNDLEN] = _inStrLen";
  if (args.out.needStruct)
    out << ",\n.pw[IPW_SNDPTR] = (uintptr_t) &_out";

  out << ",\n.pw[IPW_EPID] = _env->epID";
  if (targetArch->wordBytes == 4)
    out << ",\n.pw[IPW_EPID+1] = (_env->epID >> 32)";

  out.less();
  out << "\n};\n";

  /* NOTE: The system call protocol specifies the input values of %ecx
   * and %edx, but does not specify how return will occur. If return
   * occurs via SYSRET, the returned %ecx and %edx values will be
   * swapped.
   *
   * There is apparently no way to advise GCC that %ecx has been
   * killed on return, because it is not possible to specify that it
   * is either call clobbered or mutated in the output specification.
   * I am dealing with the issue here by introducing a volatile dummy
   * variable, but I consider this a bad option.
   *
   * In the general invoke_capability syscall, GCC got very confused
   * in non-optimizing mode when it found that no general-purpose
   * registers were available. It is moderately surprising that we
   * haven't hit this case yet in any IDL-generated code. This MAY be
   * because the parameter block was passed as an argument in the
   * general call, and lives on the stack here. It might also be the
   * case that we simply haven't seen a procedure that sends and
   * returns 4 data word yet.
   *
   * If we DO hit this issue, the thing to do is to remove the /dummy/
   * variable and push/pop %ecx so that there is a non-clobbered
   * general purpose register across the call.
   */
  out << "volatile uintptr_t _dummy;\n";
  out << "__asm__ __volatile__ (\n";
  out.more();
  out << "\"movl %%esp,(%[pwblock])\\n\t\"\n"
      << "\"movl $1f,%%edx\\n\t\"\n"
    //      << "\"sysenter\\n1:\t\"\n"
      << "\"int $0x30\\n1:\t\"\n"
      << "\"movl (%%esp),%%esp\"\n";

  out << ": /* outputs */\n";
  out.more();
  out << "\"=m\" (_softPW)";
  out << ",\n[pwblock] \"=c\" (_dummy)";
  out << ",\n[pw0] \"=a\" (_opw0)";

  /* Result word 1 is unconditional for the sake of exception code: */
  out << ",\n[pw1] \"=b\" (_softPW.pw[1])";

  /* Following must be unconditional on 32-bit targets, because we
     need it for the sake of the upper bits of the exception code: */
  if ((targetArch->wordBytes == 4) || args.out.nReg > 2)
    out << ",\n[pw2] \"=S\" (_softPW.pw[2])";
  if (args.out.nReg > 3)
    out << ",\n[pw3] \"=D\" (_softPW.pw[3])";
  out << "\n";
  out.less();

  out << ": /* inputs */\n";
  out.more();
  out << "\"[pwblock]\" (&_softPW), \"m\" (_softPW)";
  out << ",\n\"[pw0]\" (_pw0)";
  out << ",\n\"[pw1]\" (OC_" << s->QualifiedName('_') << ")";
  if (args.in.nReg > 2) {
    if ((targetArch->wordBytes == 4) || args.out.nReg > 2)
      out << ",\n\"[pw2]\" (_pw2)"; // must match input reg
    else
      out << ",\n[pw2] \"S\" (_pw2)";
  }
  if (args.in.nReg > 3) {
    if (args.out.nReg > 2)
      out << ",\n\"[pw3]\" (_pw3)"; // must match input reg
    else
      out << ",\n[pw3] \"D\" (_pw3)";
  }
  out << "\n";
  out.less();

  out << ": /* clobbers */ \"dx\", \"memory\"\n";

  out.less();
  out << ");\n";

#ifdef OBSOLETE_BUT_PRESERVED_FOR_REFERENCE
  /* Handle all non-registerized things first, since this will
     free registers for use by later instructions. Need to do
     these one at a time because offsetof() cannot process
     expressions: */

  {
    const static size_t offsets[] = 
      { 0, 0, 0, 0, 
	offsetof(i386_softregs_t, pw4),
	offsetof(i386_softregs_t, pw5),
	offsetof(i386_softregs_t, pw6),
	offsetof(i386_softregs_t, pw7) };

    for (size_t pw = 4; pw < regArgWords; pw++) {
      out << "__asm__ (\"movl %[ipw" << pw << "], %%gs:"
	  << offsets[pw]
	  << "\""
	  << " : : [ipw" << pw << "] \"r\" (_pw" << pw << "));\n";
    }
  }

  {
    const static size_t offsets[] = 
      { offsetof(i386_softregs_t, cdest[0]),
	offsetof(i386_softregs_t, cdest[1]),
	offsetof(i386_softregs_t, cdest[2]),
	offsetof(i386_softregs_t, cdest[3]) };

    for (size_t i = FIRST_CAP_REG; i < MAX_CAP_REG; i++)
      if (capResults.size() > i)
	out << "__asm__ (\"movl %[cdest" << i << "], %%gs:"
	    << offsets[i]
	    << "\""
	    << " : : [cdest" << i << "] \"r\" ("
	    << capResults[i]->name << "));\n";
  }

  {
    const static size_t offsets[] = 
      { offsetof(i386_softregs_t, csrc[0]),
	offsetof(i386_softregs_t, csrc[1]),
	offsetof(i386_softregs_t, csrc[2]),
	offsetof(i386_softregs_t, csrc[3]) };

    out << "__asm__ (\"movl %[csrc0" << "], %%gs:"
	<< offsets[0]
	<< "\""
	<< " : : [csrc0" << "] \"r\" (_env->replyCap));\n";

    for (size_t i = 0; i < capArgs.size(); i++)
      out << "__asm__ (\"movl %[csrc" << i+1 << "], %%gs:"
	  << offsets[i+1]
	  << "\""
	  << " : : [csrc" << i+1 << "] \"r\" ("
	  << capArgs[i]->name << "));\n";
  }

  out << "__asm__ (\"movl %[rcvBound], %%gs:"
      << offsetof(i386_softregs_t, rcvBound)
      << "\""
      << " : : [rcvBound] \"r\" (_outStrLen));\n";
  if (ptrForResults)
    out << "__asm__ (\"movl %[rcvPtr], %%gs:"
	<< offsetof(i386_softregs_t, rcvPtr)
	<< "\""
	<< " : : [rcvPtr] \"r\" (_out));\n";
  out << "__asm__ (\"movl %[sndLen], %%gs:"
      << offsetof(i386_softregs_t, sndLen)
      << "\""
      << " : : [sndLen] \"r\" (_inStrLen));\n";
  if (ptrForArgs)
    out << "__asm__ (\"movl %[sndPtr], %%gs:"
	<< offsetof(i386_softregs_t, sndPtr)
	<< "\""
	<< " : : [sndPtr] \"r\" (_in));\n";

  out << "volatile uintptr_t _clobbered_cx;\n";
  out << "__asm__ __volatile__ (\n";
  out.more();

  /* Emit the actual instructions: */

  //       if (regArgWords > 1)
  // 	out << "\"movl %[ipw3], %edi\\n\t\"\n";
  //       if (regArgWords > 0)
  // 	out << "\"movl %[ipw2], %esi\\n\t\"\n";

  //out << "\"movl %[ipw1], %ebx\\n\t\"\n";
  //out << "\"movl %[ipw0], %eax\\n\t\"\n";
  //out << "\"movl %[invCap], %ebp\\n\t\"\n";

  out << "\"pushl %%ebp\\n\t\"\n";
  out << "\"movl %%ecx,%%ebp\\n\t\"\n";
  out << "\"movl %%esp,%%ecx\\n\t\"\n";
  out << "\"movl $1f,%%edx\\n\t\"\n";
  out << "\"sysenter\\n1:\t\"\n";
  out << "\"popl %%ebp\"\n";

  out << ": /* outputs */\n";
  out.more();
  out << "[clobbered_cx] \"=c\" (_clobbered_cx)";
  out << ",\n[opw0] \"=a\" (_opw0)";
  out << ",\n[opw1] \"=b\" (_errCode)";
  if (args.out.nReg > 2)
    out << ",\n[opw2] \"=S\" (*_opw2)";
  if (args.out.nReg > 3)
    out << ",\n[opw3] \"=D\" (*_opw3)";
  out << "\n";
  out.less();

  out << ": /* inputs */\n";
  out.more();
  /* Use %ecx here, because there isn't an asm constraint for %bp */
  out << "[invCap] \"c\" (_invCap)";
  out << ",\n[ipw0] \"a\" (_pw0)";
  out << ",\n[ipw1] \"b\" (OC_" << s->QualifiedName('_') << ")";
  if (regArgWords > 2)
    out << ",\n[pw2] \"S\" (_pw2)";
  if (regArgWords > 3)
    out << ",\n[pw3] \"D\" (_pw3)";
  out << "\n";
  out.less();

  out << ": /* clobbers */\n";
  out.more();
  out << "\"dx\", \"memory\"";
  out.less();
  out.less();
  out << ");\n";

  {
    const static size_t offsets[] = 
      { 0, 0, 0, 0, 
	offsetof(i386_softregs_t, pw4),
	offsetof(i386_softregs_t, pw5),
	offsetof(i386_softregs_t, pw6),
	offsetof(i386_softregs_t, pw7) };

    for (size_t pw = 4; pw < args.out.nReg; pw++) {
      out << "__asm__ (\"movl %%gs:"
	  << offsets[pw]
	  << ", %[opw" << pw << "]\""
	  << " : : [opw" << pw << "] \"r\" (*_opw" << pw << "));\n";
    }
  }

  out << "\n";
  break;
#endif /* OBSOLETE_BUT_PRESERVED_FOR_REFERENCE */
}
#endif

static void
emit_target_syscall(GCPtr<Symbol> s, INOstream& out, ArgInfo& args)
{
  // Set up IPW0:

  out << "_params.in._icw = ";
  out << "IPW0_SP "
      << "| IPW0_MAKE_NR(sc_InvokeCap) | IPW0_MAKE_LDW("
      << args.in.nReg - 1
      << ")\n";
  out.more();
  if (s->cls != sc_oneway)
    out << "| IPW0_CW|IPW0_RC|IPW0_RP|IPW0_CO\n";

  /* Sending caps unconditionally, because need to send reply cap */
  out << "| IPW0_SC | IPW0_MAKE_LSC("
      << args.in.caps.size() 	// add one for reply cap
      << ")\n";

  if (args.out.caps.size() && s->cls != sc_oneway)
    out << "| IPW0_AC | IPW0_MAKE_LRC("
	<< args.out.caps.size() - 1
	<< ")\n";
  out << ";\n";
  out.less();
  out << "\n";

  if (s->cls == sc_oneway) {
    out << "invoke_capability(&_params.pb);\n";
    return;
  }

  out << "if (!invoke_capability(&_params.pb)) {\n";
  out.more();
  out << "_env->errCode = _params.except.exceptionCode;\n";
  out << "return false;\n";
  out.less();
  out << "}\n";
  out << "\n";

  out << "_env->pp = _params.out._pp;\n";
  out << "_env->epID = _params.out._epID;\n";
  out << "\n";
}

static void
emit_client_stub(GCPtr<Symbol> s, INOstream& out)
{
  {
    size_t indent = out.indent_for_macro();
    out << "#ifndef __KERNEL__\n";
    out.indent(indent);
  }

  /***************************************************************
   * Function return value, name, and parameters
   ***************************************************************/

  ArgInfo args;

  extract_args(s, args);
  emit_marshall_decl(s, out, args);

  /** @bug: What about one-ways? Should they return bool or void? */
  bool declOnly = (s->flags & (SF_NO_OPCODE|SF_NOSTUB));

  if (declOnly)
    out << "extern bool\n";
  else
    out << "static inline bool\n";

  out << "IDL_ENV_" << s->QualifiedName('_')
      << "(caploc_t _invCap";

  for(size_t i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> child = s->children[i];

    bool wantPtr = (child->cls == sc_outformal) || c_byreftype(child->type);
    wantPtr = wantPtr && !child->type->IsInterface();

    out << ", ";
    output_c_type(child->type, out);
    out << " ";
    if (wantPtr)
      out << "*";
    out << child->name;

    /* Normally would call output_c_type_trailer, but that is only
       used to add the trailing "[size]" to vectors, and we don't want
       that in the procedure signature. 

       output_c_type_trailer(child->type, out);
    */
  }

  if (!s->type->IsVoidType()) {
    GCPtr<Symbol> retType = s->type->ResolveType();
    bool wantPtr = !retType->IsInterface();

    out << ", ";
    output_c_type(retType, out);
    out << " ";
    if (wantPtr)
      out << "*";
    out << "_retVal";
  }

  out << ", IDL_Environment *_env)\n";

  for(size_t i = 0; i < s->raises.size(); i++) {
    out.more();
    out << "/* raises RC_"
	<< s->raises[i]->QualifiedName('_')
	<< " */\n";
    out.less();
  }

  if (declOnly) {
    out << ";\n";

  } else {
    out << "{\n";
    out.more();
    
    /* In principal, the input and output structures could be placed
       within a union. This becomes complicated when we do dynamic
       indirect arguments, and few stubs have direct components so large
       that saving the extra stack words is worthwhile. For this reason
       we do without the union here. */
    out << "_INV_" << s->QualifiedName('_') << " _params;\n";
    
    emit_in_marshall(s, out, args.in);
    emit_out_marshall(s, out, args.out);
    
    // What about cap marshall?
    
    emit_target_syscall(s, out, args);
    
    if (s->cls != sc_oneway) {
      size_t nHardReg = MAX_DATA_REG;
      if (targetArch->archID == COYOTOS_ARCH_i386)
	nHardReg = 4;
      emit_out_demarshall(s, out, args.out, nHardReg);
    }
    
    
    /*  if (!s->type->IsVoidType()) */
    out << "return true;\n";
    
    out.less();
    out << "}\n";
  }


  if (declOnly)
    out << "extern bool\n";
  else
    out << "static inline bool\n";

  out << "" << s->QualifiedName('_')
      << "(caploc_t _invCap";

  for(size_t i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> child = s->children[i];

    bool wantPtr = (child->cls == sc_outformal) || c_byreftype(child->type);
    wantPtr = wantPtr && !child->type->IsInterface();

    out << ", ";
    output_c_type(child->type, out);
    out << " ";
    if (wantPtr)
      out << "*";
    out << child->name;

    /* Normally would call output_c_type_trailer, but that is only
       used to add the trailing "[size]" to vectors, and we don't want
       that in the procedure signature. 

       output_c_type_trailer(child->type, out);
    */
  }

  if (!s->type->IsVoidType()) {
    GCPtr<Symbol> retType = s->type->ResolveType();
    bool wantPtr = !retType->IsInterface();

    out << ", ";
    output_c_type(retType, out);
    out << " ";
    if (wantPtr)
      out << "*";
    out << "_retVal";
  }

  out << ")\n";

  for(size_t i = 0; i < s->raises.size(); i++) {
    out.more();
    out << "/* raises RC_"
	<< s->raises[i]->QualifiedName('_')
	<< " */\n";
    out.less();
  }

  if (declOnly) {
    out << ";\n";
  } else {
    out << "{\n";
    out.more();
    out << "return IDL_ENV_" << s->QualifiedName('_') 
	<< "(_invCap";
    for(size_t i = 0; i < s->children.size(); i++) {
      GCPtr<Symbol> child = s->children[i];

      out << ", " << child->name;
    }

    if (!s->type->IsVoidType())
      out << ", " << "_retVal";

    out << ", __IDL_Env);\n";

    out.less();
    out << "}\n";
  }

  {
    size_t indent = out.indent_for_macro();
    out << "#endif /* !__KERNEL__ */\n";
    out.indent(indent);
  }
}

static void
emit_server_if_union(GCPtr<Symbol> s, INOstream& out)
{
  out << "typedef union {\n";
  out.more();

  for (GCPtr<Symbol> ifsym = s; ifsym; ifsym = ifsym->baseType) {
    for(size_t i = 0; i < ifsym->children.size(); i++) {
      GCPtr<Symbol> child = ifsym->children[i];

      if (child->cls != sc_operation && child->cls != sc_oneway)
	continue;

      if (child->flags & SF_NO_OPCODE)
	continue;

      out << "_INV_" << child->QualifiedName('_') 
	  << " " << child->name << ";\n";
    }
  }

  out << "InvParameterBlock_t _pb;\n";
  out << "InvExceptionParameterBlock_t except;\n";
  out.less();
  out << "} _IDL_IFUNION_" << s->QualifiedName('_') << ";\n";
  out << "\n";
}

static void
emit_server_if_dispatch_proc(GCPtr<Symbol> s, INOstream& out)
{
  out << "static inline void\n"
      << "_IDL_IFDISPATCH_" << s->QualifiedName('_') << "(\n"
      << "    _IDL_IFUNION_" << s->QualifiedName('_') << " *_params,\n"
      << "    struct IDL_SERVER_Environment *_env)\n"
      << "{\n";
  {
    out.more();
    out << "switch(_params->_pb.pw[IPW_OPCODE]) {\n";
    for (GCPtr<Symbol> ifsym = s; ifsym; ifsym = ifsym->baseType) {
      for(size_t i = 0; i < ifsym->children.size(); i++) {
	GCPtr<Symbol> child = ifsym->children[i];

	if (child->cls != sc_operation && child->cls != sc_oneway)
	  continue;

	if (child->flags & SF_NO_OPCODE)
	  continue;

	ArgInfo args;
	extract_args(child, args);


	out << "case OC_" << child->QualifiedName('_') <<":\n";
	{
	  out.more();
	  out << "{\n";
	  out.more();
	  out << "_IDL_DEMARSHALL_" << child->QualifiedName('_') << "(\n"
	      << "    &_params->" << child->name << ", _env,\n"
	      << "    HANDLE_" << child->QualifiedName('_');


	  if (args.out.indirectBytes)
	    out << ",\n"
		<< "    CLEANUP_" << child->QualifiedName('_');
	  out << ");\n"
	      << "break;\n";
	  out.less();
	  out << "}\n";
	  out.less();
	}
      }
    }
    out << "default:\n";
    {
      out.more();
      out << "{\n";
      out.more();
      out << "_params->except.icw =\n"
	  << "  IPW0_MAKE_LDW((sizeof(_params->except)/sizeof(uintptr_t))-1)\n"
	  << "  |IPW0_EX|IPW0_SP\n;"
	  << "_params->except.exceptionCode = RC_coyotos_Cap_UnknownRequest;\n"
	  << "break;\n";
      out.less();
      out << "}\n";
      out.less();
    }
    out << "}\n";
    out.less();
    out << "}\n";
    out << "\n";
  }
}

static void
emit_server_in_demarshall_arg(GCPtr<Symbol> s, INOstream& out)
{
  GCPtr<Symbol> argBaseType = s->type->ResolveType();

  if (argBaseType->IsFixSequenceType()) {
    output_c_type(s->type, out);
    out << " ";
    out << s->name;
    output_c_type_trailer(s->type, out);
    out << ";\n";

    BigNum bound = compute_value(argBaseType->value);
    out << "__builtin_memcpy("
	<< s->name
	<< ", &_params.server_in."
	<< s->name
	<< ", "
	<< s->name
	<< ", sizeof("
	<< s->name
	<< "[0]) * " << bound << ");\n";
  }

  if (argBaseType->IsSequenceType()) {
    BigNum bound = compute_value(argBaseType->value);

    out << "/* Indirect Component: */\n";
    out << "{\n";
    {
      out.more();
      out << "_params->server_in." << s->name
	  << ".data = (typeof("
	  << "_params->server_in." << s->name
	  << ".data)) &_params->server_in._indirect[_inIndirNdx];\n";
      out << "_params->server_in." << s->name
	  << ".max = " << bound << ";\n";
      out << "size_t _nBytes = sizeof("
	  << "_params->server_in." << s->name << ".data[0]) * "
	  << "_params->server_in." << s->name << ".len;\n"
	  << "_nBytes = IDL_ALIGN_TO(_nBytes, sizeof(uintptr_t));\n"
	  << "_inIndirNdx += _nBytes;\n";
      out.less();
    }
    out << "}\n";
  }
}

static void
emit_server_op_in_demarshall(GCPtr<Symbol> s, UniParams& args, INOstream& out)
{
  if (! (args.regs.size() || args.strings.size()) )
    return;

  if (args.indirectBytes) {
    out << "size_t _inIndirNdx = 0;\n";
    out << "\n";
  }

  /* There actually isn't much in demarshalling to do. Mostly, we need
     to reconstruct the data pointers for sequence types. */

  for(size_t i = 0; i < args.regs.size(); i++)
    emit_server_in_demarshall_arg(args.regs[i], out);

  for(size_t i = 0; i < args.strings.size(); i++)
    emit_server_in_demarshall_arg(args.strings[i], out);

  out << "\n";
}

static void
emit_server_out_prep_result(GCPtr<Symbol> s, INOstream& out)
{
  GCPtr<Symbol> argBaseType = s->type->ResolveType();
  if (argBaseType->IsSequenceType()) {
    size_t bound = argBaseType->indirectSize(*targetArch);

    output_c_type(s->type, out);
    out << " ";
    out << s->name << " = { " << bound << ", 0, 0 };\n";
  }
}

static void
emit_server_op_out_prep(GCPtr<Symbol> s, UniParams& args, INOstream& out)
{
  if (s->cls == sc_oneway)
    return;

  if (! (args.indirectBytes) )
    return;

  /* There actually isn't much in demarshalling to do. Mostly, we need
     to reconstruct the data pointers for sequence types. */

  for(size_t i = 0; i < args.regs.size(); i++)
    emit_server_out_prep_result(args.regs[i], out);

  for(size_t i = 0; i < args.strings.size(); i++)
    emit_server_out_prep_result(args.strings[i], out);

  out << "\n";
}

static void
emit_server_out_marshall_result(GCPtr<Symbol> s, INOstream& out)
{
  GCPtr<Symbol> argBaseType = s->type->ResolveType();
  if (argBaseType->IsSequenceType()) {
    out << "_params->server_out." << s->name << " = " << s->name << ";\n";

    out << "_params->server_out." << s->name
	<< ".data = (typeof("
	<< s->name
	<< ".data)) &_params->server_out._indirect[_outIndirNdx];\n";

    out << "size_t _nBytes = sizeof("
	<< s->name << ".data[0]) * "
	<< s->name << ".len;\n";

    out << "__builtin_memcpy("
	<< "&_params->server_out._indirect[_outIndirNdx], "
	<< s->name
	<< ".data, "
	<< "_nBytes);\n";

    out << "_outIndirNdx += IDL_ALIGN_TO(_nBytes, sizeof(uintptr_t));\n";
  }
}
static void
emit_server_op_out_marshall(GCPtr<Symbol> s, UniParams& args, INOstream& out)
{
  if (s->cls == sc_oneway) {
    // server_out doesn't exist for oneways, so we use server_in.
    out << "/* No reply expected, so clear _icw and sndLen */\n"
	<< "_params->server_in._icw = 0;\n"
	<< "_params->server_in._sndLen = 0;\n";
    return;
  }

  if (args.indirectBytes) {
    out << "\n";
    out << "size_t _outIndirNdx = 0;\n";
  }

  out << "\n";

  /* There actually isn't much in demarshalling to do. Mostly, we need
     to reconstruct the data pointers for sequence types. */

  for(size_t i = 0; i < args.regs.size(); i++)
    emit_server_out_marshall_result(args.regs[i], out);

  for(size_t i = 0; i < args.strings.size(); i++)
    emit_server_out_marshall_result(args.strings[i], out);

  if (args.strings.size() && args.indirectBytes) {
    out << "_params->server_out._sndPtr = &_params->server_out."
	<< args.strings[0]->name << ";\n";
    out << "_params->server_out._sndLen = offsetof(typeof(_params->out), _indirect) - "
	<< "offsetof(typeof(_params->out), "
	<< args.strings[0]->name << ") + _outIndirNdx;\n";
  }
  else if (args.strings.size()) {
    out << "_params->server_out._sndPtr = &_params->server_out."
	<< args.strings[0]->name << ";\n";
    out << "_params->server_out._sndLen = sizeof(_params->out) - "
	<< "offsetof(typeof(_params->out), "
	<< args.strings[0]->name << ");\n";
  }
  else if (args.indirectBytes) {
    out << "_params->server_out._sndPtr = &_params->server_out._indirect;\n";
    out << "_params->server_out._sndLen = _outIndirNdx;\n";
  }
  else
    out << "_params->server_out._sndLen = 0;\n";

  out << "_params->server_out._icw = "
      << "IPW0_SP|IPW0_MAKE_NR(sc_InvokeCap)|IPW0_MAKE_LDW("
      << args.nReg - 1
      << ");\n";
  if (args.caps.size()) {
    out << "_params->server_out._icw |= IPW0_SC|IPW0_MAKE_LSC("
	<< args.caps.size() - 1
	<< ");\n";
  }
}

static void
emit_server_op_handler_call(GCPtr<Symbol> s, ArgInfo& args, INOstream& out)
{
  std::string retType = (s->cls == sc_oneway) ? "void" : "uint64_t";

  if (s->cls == sc_oneway)
    out	<< "(void) _handler(\n";
  else
    out << "uint64_t _result = _handler(\n";

  {
    out.more();

    for (size_t i = 0; i < s->children.size(); i++) {
      GCPtr<Symbol> child = s->children[i];
      GCPtr<Symbol> baseType = child->type->ResolveType();

      bool wantPtr = (child->cls == sc_outformal);
      wantPtr = wantPtr && !baseType->IsInterface();
      wantPtr = wantPtr && !baseType->IsFixSequenceType();

      if (wantPtr) {
	out << '&';
      }

      if (child->cls == sc_formal) {
	/* Fixed sequence types get manually copied before passing because
	   they would otherwise not be passed by value, and the
	   overwrite of an out parameter might clobber them. */
	if (!baseType->IsFixSequenceType())
	  out << "_params->server_in.";
      }
      else {
	/* For outbound sequence types, we establish a local temporary. We
	   can't prep them in-place for fear of overwriting an IN
	   parameter before it is copied. */
	if (!baseType->IsSequenceType())
	  out << "_params->server_out.";
      }

      out << child->name << ",\n";
    }

    if (!s->type->IsVoidType()) {
      GCPtr<Symbol> baseType = s->type->ResolveType();

      bool wantPtr = true;	// this is an OUT parameter
      wantPtr = wantPtr && !baseType->IsInterface();
      wantPtr = wantPtr && !baseType->IsFixSequenceType();

      if (wantPtr) out << '&';

      if (!baseType->IsSequenceType())
	out << "_params->server_out.";

      out << "_retVal,\n";
    }

    out << "_env";
    out.less();
  }

  out << ");\n";
  out << "\n";
}

static void
emit_cleanup_decl(const std::string& nm, GCPtr<Symbol> s, ArgInfo& args, INOstream& out)
{
  out << "void " << nm << "(\n";
  {
    out.more();

    for(size_t i = 0; i < s->children.size(); i++) {
      GCPtr<Symbol> child = s->children[i];

      if (child->cls != sc_outformal)
	continue;

      if (child->type->IsSequenceType()) {
	output_c_type(child->type, out);
	out << " ";
	out << child->name;
	out << ",\n";
      }
    }

    if (s->type->IsSequenceType()) {
      output_c_type(s->type, out);
      out << " _retVal,\n";
    }

    out << "struct IDL_SERVER_Environment *_env)";
    out.less();
  }
}

static void
emit_cleanup_call(GCPtr<Symbol> s, ArgInfo& args, INOstream& out)
{
  if (args.out.indirectBytes == 0)
    return;

  out << "\n"
      << "_cleanup(\n";
  {
    out.more();

    for(size_t i = 0; i < s->children.size(); i++) {
      GCPtr<Symbol> child = s->children[i];

      if (child->cls != sc_outformal)
	continue;

      if (child->type->IsSequenceType()) {
	out << child->name;
	out << ",\n";
      }
    }

    if (s->type->IsSequenceType())
      out << " _retVal,\n";

    out << "_env);\n";
    out.less();
  }
}

static void
emit_server_handler_decl(const std::string& nm, 
			 GCPtr<Symbol> s, ArgInfo& args, INOstream& out,
			 bool mainDecl)
{
  if (mainDecl)
    out << "IDL_SERVER_HANDLER_PREDECL ";

  std::string retType = (s->cls == sc_oneway) ? "void" : "uint64_t";
  out << retType << " " << nm << "(\n";
  out.more();
  for(size_t i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> child = s->children[i];

    bool wantPtr = (child->cls == sc_outformal) || c_byreftype(child->type);
    wantPtr = wantPtr && !child->type->IsInterface();

    output_c_type(child->type, out);
    out << " ";
    if (wantPtr)
      out << "*";
    out << child->name;
    out << ",\n";

    /* Normally would call output_c_type_trailer, but that is only
       used to add the trailing "[size]" to vectors, and we don't want
       that in the procedure signature. 

       output_c_type_trailer(child->type, out);
    */
  }

  if (!s->type->IsVoidType()) {
    GCPtr<Symbol> retType = s->type->ResolveType();

    bool wantPtr = !retType->IsInterface();

    output_c_type(retType, out);
    out << " ";
    if (wantPtr)
      out << "*";
    out << "_retVal,\n";
  }

  out << "struct IDL_SERVER_Environment *_env)";
  out.less();
}

static void
emit_server_op_demarshall_proc(GCPtr<Symbol> s, ArgInfo& args, INOstream& out)
{
  /** @bug: What to do about NOSTUB demarshalling? */
  if (s->flags & SF_NOSTUB) {
    out << "/* Proc " << s->QualifiedName('.') << " is marked NOSTUB */\n";
    return;
  }

  emit_server_handler_decl("HANDLE_"+s->QualifiedName('_'),
			   s, args, out, true);
  out << ";\n";
  if (args.out.indirectBytes) {
    emit_cleanup_decl("CLEANUP_"+s->QualifiedName('_'),
		      s, args, out);
    out << ";\n";
  }
  out << "\n";

  out << "/* Demarshall proc for " << s->QualifiedName('.') << " */\n";
  out << "\n";

  assert(out.depth == 0);
  out << "static inline void\n"
      << "_IDL_DEMARSHALL_" << s->QualifiedName('_') << "(\n";
  {
    out.more();
    out << "_INV_" << s->QualifiedName('_') << " *_params,\n";
    out << "struct IDL_SERVER_Environment *_env,\n";

    /* Emit variant of the actual handler procedure signature: */

    emit_server_handler_decl("(*_handler)", s, args, out, false);
    if (args.out.indirectBytes) {
      out << ",\n";
      emit_cleanup_decl("(*_cleanup)", s, args, out);
    }

    out << ")\n";

    out.less();
  }

  out << "{\n";
  {
    out.more();
    out << "/* FIX: Need to sanity check input argument counts in ldw, lsc, SC.\n"
	<< " * Can only do a partial check on sndLen, because there may be a variable\n"
	<< " * component to that.\n"
	<< " */\n"
	<< "\n";


    /* Ironically, the string based parameters are easier to deal with
       than the register based parameters, because they can be passed
       and received in place.

       CAP parameters can go in place.

       IN  Register parameters that are scalars can be handled in-place.

       IN  Register parameters that were marshalled require temporaries.
       This includes all cases that might involve an indirect string.

       OUT register parameters require temporaries.
    */

    emit_server_op_in_demarshall(s, args.in, out);
    emit_server_op_out_prep(s, args.out, out);

    emit_server_op_handler_call(s, args, out);

    if (s->cls != sc_oneway) {
      out << "if (_result != RC_coyotos_Cap_OK) {\n";
      {
	out.more();
	emit_cleanup_call(s, args, out);
	out << "_params->except.icw = "
	    << "IPW0_MAKE_LDW((sizeof(_params->except)/sizeof(uintptr_t))-1) "
	    << "| IPW0_EX;\n"
	    << "_params->except.exceptionCode = _result;\n"
	    << "return;\n";
	out.less();
      }
      out << "}\n";
    }
    emit_server_op_out_marshall(s, args.out, out);

    emit_cleanup_call(s, args, out);

    out.less();
  }
  out << "}\n";

  out << "\n";
}

static void
client_header_symdump(GCPtr<Symbol> s, INOstream& out)
{
  if (s->docComment)
    PrintDocComment(s->docComment, out);

  switch(s->cls) {
  case sc_package:
    {
      /* Don't output nested packages. */
      if (s->nameSpace->cls == sc_package)
	return;

      {
	size_t indent = out.indent_for_macro();
	out << "#ifndef __"
	    << s->QualifiedName('_')
	    << "__ /* package "
	    << s->QualifiedName('.')
	    << " */\n";
	out << "#define __"
	    << s->QualifiedName('_')
	    << "__\n";
	out.indent(indent);
      }

      for(size_t i = 0; i < s->children.size(); i++) {
	out.more();
	client_header_symdump(s->children[i], out);
	out.less();
      }

      out << '\n';

      {
	size_t indent = out.indent_for_macro();
	out << "#endif /* __"
	    << s->QualifiedName('_')
	    <<"__ */\n";
	out.indent(indent);
      }
      break;
    }
  case sc_scope:
    {
      out << "\n";
      out << "/* namespace "
	  << s->QualifiedName('_')
	  << " { */\n";

      for(size_t i = 0; i < s->children.size(); i++) {
	out.more();
	client_header_symdump(s->children[i], out);
	out.less();
      }

      out << "/* } ; */\n";

      break;
    }

    /*  case sc_builtin: */

    /*
     * NOTE:
     * CapIDL structs seem to be getting called as though they were
     * typedef structs so this checks to see if the case is for sc_struct
     * and adds a typedef accordingly.  It might also be possible to
     * add struct into the function calls, however, since all of the
     * calls don't include struct, it seems intuitively obvious that
     * this ought to add typedef.  I could be wrong.  Fortunately,
     * it's not hard to change
     */

  case sc_enum:
  case sc_bitset:
    {
      out << "\n";

      print_asmifdef(out);

      out << "typedef ";
      output_c_type(s->type, out);
      out << ' ' << s->QualifiedName('_') << ";\n";

      print_asmendif(out);

      for(size_t i = 0; i < s->children.size(); i++)
	client_header_symdump(s->children[i], out);

      break;
    }

  case sc_struct:
    {
      out << "\n";
      print_asmifdef(out);

      out << "typedef "
	  << s->ClassName()
	  << " "
	  << s->QualifiedName('_')
	  << " {\n";

      for(size_t i = 0; i < s->children.size(); i++) {
	out.more();
	client_header_symdump(s->children[i], out);
	out.less();
      }

      out << "} "
	  << s->QualifiedName('_')
	  << ";\n";
      print_asmendif(out);
      break;
    }
  case sc_exception:
    {
      uint64_t sig = s->CodedName();

      {
	size_t indent = out.indent_for_macro();
	out << "#define RC_"
	    << s->QualifiedName('_')
	    << " CAPIDL_U64(0x"
	    << std::hex << sig << std::dec
	    << ")\n";
	out.indent(indent);
      }

      /* Exceptions only generate a struct definition if they 
	 actually have members. */
      if (s->children.size() > 0) {
	out << "\n";
	print_asmifdef(out);

	out << "struct "
	    << s->QualifiedName('_')
	    << " {\n";

	for(size_t i = 0; i < s->children.size(); i++) {
	  out.more();
	  client_header_symdump(s->children[i], out);
	  out.less();
	}

	out << "} ;\n";
	print_asmendif(out);
      }

      break;
    }

  case sc_interface:
  case sc_absinterface:
    {
      uint64_t sig = s->CodedName();

      out << "\n";
      out << "/* BEGIN Interface "
	  << s->QualifiedName('.')
	  << " */\n";
      
      /* Identify the exceptions raised at the interface level first: */
      for(size_t i = 0; i < s->raises.size(); i++) {
	out.more();
	out << "/* interface raises "
	    << s->raises[i]->QualifiedName('_')
	    << " */\n";
	out.less();
      }

      out << "\n";

      {
	size_t indent = out.indent_for_macro();
	out << "#define IKT_"
	    << s->QualifiedName('_')
	    << " CAPIDL_U64(0x"
	    << std::hex << sig << std::dec
	    << ")\n\n";
	out.indent(indent);
      }

      print_asmifdef(out);
      out << "typedef caploc_t "
	  << s->QualifiedName('_')
	  << ";\n";
      print_asmendif(out);
      out << "\n";

      for(size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];

	client_header_symdump(child, out);
      }

      out << "\n";

      out << "/* END Interface "
	  << s->QualifiedName('.')
	  << " */\n";

      break;
    }

  case sc_symRef:
    {
      out << "/* symref "
	  << s->name
	  << " */";
      out << s->value->QualifiedName('_');
      break;
    }

  case sc_typedef:
    {
      print_asmifdef(out);
      out << "typedef ";
      output_c_type(s->type, out);
      out << ' ';
      out << s->QualifiedName('_');
      output_c_type_trailer(s->type, out);
      out << ";\n";
      print_asmendif(out);
      break;
    }

  case sc_union:
    {
      out << "\n";
      print_asmifdef(out);

      out << "/* tagged union */\n";

      out << "typedef "
	  << "struct"
	  << " "
	  << s->QualifiedName('_')
	  << " {\n";

      out.more();
      output_c_type(s->type, out);
      out << " tag;\n";
      /* No need for C type trailer, since the type must be a scalar
	 and those do not have trailers. */
      out << "union {\n";
      out.more();

      for(size_t i = 0; i < s->children.size(); i++)
	client_header_symdump(s->children[i], out);

      out.less();
      out << "} u;\n";
      out.less();

      out << "} "
	  << s->QualifiedName('_')
	  << ";\n";

      print_asmendif(out);
      break;
    }

  case sc_caseScope:
    {
      for(size_t i = 0; i < s->children.size(); i++)
	client_header_symdump(s->children[i], out);
      break;
    }

  case sc_caseTag:
    {
      out << "/* CASE " << s->name << " */\n";
      break;
    }

  case sc_switch:
    {
      out << "/* cls "
	  << s->ClassName()
	  << " "
	  << s->QualifiedName('_')
	  << " */\n";

      for(size_t i = 0; i < s->children.size(); i++) {
	out.more();
	client_header_symdump(s->children[i], out);
	out.less();
      }

      break;
    }
  case sc_primtype:
    {
      output_c_type(s, out);
      break;
    }
  case sc_oneway:
  case sc_operation:
    {
      if (s->flags & SF_NO_OPCODE) {
	out << "\n/* Method "
	    << s->QualifiedName('_')
	    << " is client-only */\n";
      }
      else {
	uint32_t sig = s->CodedName();
	size_t indent = out.indent_for_macro();
	out << "\n#define OC_"
	    << s->QualifiedName('_')
	    << " CAPIDL_U32(0x"
	    << std::hex << sig << std::dec
	    << ")\n";
	out.indent(indent);
      }

      print_asmifdef(out);

      emit_client_stub(s, out);

      print_asmendif(out);
      break;
    }

  case sc_arithop:
    {
      out << '(';

      out.more();
      if (s->children.size() > 1) {
	client_header_symdump(s->children[0], out);
	out << s->name;
	client_header_symdump(s->children[1], out);
      }
      else {
	out <<s->name;
	client_header_symdump(s->children[0], out);
      }
      out.less();
      out << ')';

      break;
    }

  case sc_value:		/* computed constant values */
    {
      switch(s->v.lty){
      case lt_integer:
	{
	  std::string str = s->v.bn.asString(10);
	  out << str;
	  break;
	}
      case lt_unsigned:
	{
	  std::string str = s->v.bn.asString(10);
	  out << str << "u";
	  break;
	}
      case lt_float:
	{
	  out << s->v.d;
	  break;
	}
      case lt_char:
	{
	  std::string str = s->v.bn.asString(10);
	  out << "'" << str << "'";
	  break;
	}
      case lt_bool:
	{
	  out << (s->v.bn.as_uint32() ? "TRUE" : "FALSE");
	  break;
	}
      default:
	{
	  out << "/* lit UNKNOWN/BAD TYPE> */";
	  break;
	}
      };
      break;
    }

  case sc_const:		/* constant symbols, including enum values */
    {
      /* Issue: the C 'const' declaration declares a value, not a
	 constant. This is bad. Unfortunately, using #define does not
	 guarantee proper unification of string constants in the
	 string pool (or rather, it does according to ANSI C, but it
	 isn't actually implemented in most compilers). We therefore
	 use #define ugliness. We do NOT guard against multiple
	 definition here, since the header file overall already has
	 this guard. */
      /* FIX: Not all constants are integers. The call below to
	 mpz_get_ui needs to be replaced with a suitable type dispatch
	 on the legal literal types. */
      BigNum mi = compute_value(s->value);
      {
	size_t indent = out.indent_for_macro();
	out << "#define "
	    << s->QualifiedName('_')
	    << " ";
	out.indent(indent);
      }

      GCPtr<Symbol> ty = s->type->ResolveType();
      assert(ty);
      assert (ty->cls == sc_primtype);

      if (ty->v.lty == lt_unsigned) {
	out << "CAPIDL_U"
	    << ty->v.bn.as_uint32()
	    << '('
	    << mi
	    << ")\n";
      }
      else if (ty->v.lty == lt_integer) {
	out << "CAPIDL_I"
	    << ty->v.bn.as_uint32()
	    << '('
	    << mi
	    << ")\n";
      }
      else
	out << mi << '\n';

      break;
    }

  case sc_member:
    {
      output_c_type(s->type, out);

      out << " " << s->name;

      output_c_type_trailer(s->type, out);

      out << ";\n";

      break;
    }
  case sc_formal:
  case sc_outformal:
    {
      bool wantPtr = (s->cls == sc_outformal) || c_byreftype(s->type);
      wantPtr = wantPtr && !s->type->IsInterface();

      output_c_type(s->type, out);
      out << ' ';
      if (wantPtr)
	out << '*';
      out << s->name;
      /* calling output_c_type_trailer here, leads to some of the same
	 problems we saw in o_c_client, where a [value] was getting
	 appended to a procedure declaration.  In this case it's not
	 correct, so for now, it has been disabled.

	 output_c_type_trailer(s->type, out, 0);
      */
      break;
    }

  case sc_seqType:
    {
      out << "seqType SHOULD NOT BE OUTPUT DIRECTLY!\n";
      break;
    }

  case sc_arrayType:
    {
      out << "arrayType SHOULD NOT BE OUTPUT DIRECTLY!\n";
      break;
    }

  case sc_inhOneway:
  case sc_inhOperation:
    {
      out << "/* Inherits "
	  << s->name
	  << " */\n";
      break;
    }

  default:
    {
      out << "UNKNOWN/BAD SYMBOL TYPE "
	  << s->cls
	  << " FOR: "
	  << s->name
	  << "\n";
      break;
    }
  }
}

void
output_c_client_hdr(GCPtr<Symbol> s)
{
  Path fileName;
  Path dirName;
  SymVec vec;
  
  if (s->isActiveUOC == false)
    return;

  if (targetArch == 0) {
    fprintf(stderr, "Target architecture not specified\n");
    exit(1);
  }

  {
    std::string sqn = s->QualifiedName('/');
    fileName = (Path(target) + Path(sqn)) << ".h";
  }

  dirName = fileName.dirName();

  dirName.smkdir();

  std::ofstream outputFileStream(fileName.c_str(), std::ios_base::out|std::ios_base::trunc);

  if (!outputFileStream.is_open()) {
    fprintf(stderr, "Couldn't open stub header file \"%s\"\n",
	    fileName.c_str());
    exit(1);
  }

  INOstream out(outputFileStream);

  s->ComputeDependencies(vec);

  vec.qsort(Symbol::CompareByQualifiedName);

  /* Emit multiple inclusion guard: */
  out << "#ifndef __"
      << s->QualifiedName('_')
      << "_h__\n";
  out << "#define __"
      << s->QualifiedName('_')
      << "_h__\n";

  /* 
   * We need target.h for long long to work, however, I'm not certain this
   * is the proper place for it to go.  This ought to work in the interim.
   */

  out << "\n";
  out << "#include <coyotos/capidl.h>\n";
  out << "#include <coyotos/syscall.h>\n";

  for (size_t i = 0; i < vec.size(); i++) {
    GCPtr<Symbol> sym = vec[i];

    if (sym->QualifiedName('/') != s->QualifiedName('/'))
      out << "#include <idl/"
	  << sym->QualifiedName('/')
	  << ".h>\n";
  }

  client_header_symdump(s, out);

  /* Emit multiple inclusion guard end: */
  out << "\n";
  out << "#endif /* __"
      << s->QualifiedName('_')
      << "_h__ */\n";
}

static void
server_header_symdump(GCPtr<Symbol> s, INOstream& out)
{
  switch(s->cls) {
  case sc_package:
    {
      /* Don't output nested packages. */
      if (s->nameSpace->cls == sc_package)
	return;

      {
	size_t indent = out.indent_for_macro();
	out << "#ifndef __"
	    << s->QualifiedName('_')
	    << "__SERVER__ /* package "
	    << s->QualifiedName('.')
	    << " */\n";
	out << "#define __"
	    << s->QualifiedName('_')
	    << "__\n";
	out << "/* #include <client header> */\n";
	out.indent(indent);
      }

      for(size_t i = 0; i < s->children.size(); i++) {
	out.more();
	server_header_symdump(s->children[i], out);
	out.less();
      }

      out << '\n';

      {
	size_t indent = out.indent_for_macro();
	out << "#endif /* __"
	    << s->QualifiedName('_')
	    <<"__ */\n";
	out.indent(indent);
      }
      break;
    }
  case sc_scope:
    {
      out << "\n";
      out << "/* namespace "
	  << s->QualifiedName('_')
	  << " { */\n";

      for(size_t i = 0; i < s->children.size(); i++) {
	out.more();
	server_header_symdump(s->children[i], out);
	out.less();
      }

      out << "/* } ; */\n";

      break;
    }

    /*  case sc_builtin: */

    /* NOTE:
     *
     * Many things used on the server side rely on emission being
     * performed in the corresponding client header.
     */

  case sc_enum:
  case sc_bitset:
  case sc_struct:
  case sc_exception:
  case sc_symRef:
  case sc_typedef:
  case sc_union:
  case sc_caseScope:
  case sc_caseTag:
  case sc_switch:
  case sc_primtype:
  case sc_arithop:
  case sc_value:		/* computed constant values */
  case sc_const:		/* constant symbols, including enum values */
  case sc_member:
  case sc_formal:
  case sc_outformal:
  case sc_seqType:
  case sc_arrayType:


    /* Emitted in client header */
    break;

  case sc_interface:
  case sc_absinterface:
    {
      out << "\n";
      out << "/* BEGIN Interface "
	  << s->QualifiedName('.')
	  << " */\n";
      
      out << "\n";


      for(size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];

	server_header_symdump(child, out);
      }

      emit_server_if_union(s,  out);
      emit_server_if_dispatch_proc(s,  out);
      out << "/* END Interface "
	  << s->QualifiedName('.')
	  << " */\n";

      break;
    }

  case sc_oneway:
  case sc_operation:
    {
      if (s->flags & SF_NO_OPCODE) {
	out << "\n/* Method "
	    << s->QualifiedName('_')
	    << " is client-only */\n";
      }
      else {
	ArgInfo args;
	extract_args(s, args);

	emit_server_op_demarshall_proc(s, args, out);
      }

      // emit_server_leg_field(s, sc_formal, out);
      // emit_server_leg_field(s, sc_outformal, out);

      break;
    }

  case sc_inhOneway:
  case sc_inhOperation:
    {
      out << "/* Inherits "
	  << s->name
	  << " */\n";
      break;
    }

  default:
    {
      out << "UNKNOWN/BAD SYMBOL TYPE "
	  << s->cls
	  << " FOR: "
	  << s->name
	  << "\n";
      break;
    }
  }
}


void
output_c_server_hdr(GCPtr<Symbol> s)
{
  Path fileName;
  Path dirName;
  
  if (s->isActiveUOC == false)
    return;

  if (targetArch == 0) {
    fprintf(stderr, "Target architecture not specified\n");
    exit(1);
  }

  {
    std::string sqn = s->QualifiedName('/');
    fileName = (Path(target) + Path(sqn)) << ".server.h";
  }

  dirName = fileName.dirName();

  dirName.smkdir();

  std::ofstream outputFileStream(fileName.c_str(), std::ios_base::out|std::ios_base::trunc);

  if (!outputFileStream.is_open()) {
    fprintf(stderr, "Couldn't open stub header file \"%s\"\n",
	    fileName.c_str());
    exit(1);
  }

  INOstream out(outputFileStream);

  /* Emit multiple inclusion guard: */
  out << "#ifndef __"
      << s->QualifiedName('_')
      << "_SERVER_h__\n";
  out << "#define __"
      << s->QualifiedName('_')
      << "_SERVER_h__\n";

  /* 
   * We need target.h for long long to work, however, I'm not certain this
   * is the proper place for it to go.  This ought to work in the interim.
   */

  out << "\n";
  out << "#include <coyotos/capidl.h>\n";
  out << "#include <coyotos/syscall.h>\n";

  if (s->baseType)
    out << "#include <idl/"
	<< s->baseType->QualifiedName('/')
	<< ".server.h>\n";

  out << "#include <idl/"
      << s->QualifiedName('/')
      << ".h>\n";

  out << "\n";
  out << "struct IDL_SERVER_Environment;\n";
  out << "\n";

  out << "#ifndef IDL_SERVER_HANDLER_PREDECL\n";
  out << "#define IDL_SERVER_HANDLER_PREDECL\n";
  out << "#endif /* IDL_SERVER_HANDLER_PREDECL */\n";
  out << "\n";

  server_header_symdump(s, out);

  /* Emit multiple inclusion guard end: */
  out << "\n";
  out << "#endif /* __"
      << s->QualifiedName('_')
      << "_SERVER_h__ */\n";
}

static void
server_hdr_pkgwalker(GCPtr<Symbol> scope, SymVec& vec)
{
  /* Export subordinate packages first! */
  for (size_t i = 0; i < scope->children.size(); i++) {
    GCPtr<Symbol> child = scope->children[i];
    if (child->cls != sc_package && child->isActiveUOC)
      child->ComputeDependencies(vec);

    if (child->cls == sc_package)
      server_hdr_pkgwalker(child, vec);
  }
}

void
output_server_dependent_headers(GCPtr<Symbol> scope, BackEndFn outfn)
{
  SymVec vec;

  server_hdr_pkgwalker(scope, vec);

  vec.qsort(Symbol::CompareByQualifiedName);

  /* Use the clearing of isActiveUOC to avoid duplicate processing. */
  for(size_t i = 0; i < vec.size(); i++)
    vec[i]->isActiveUOC = true;

  for(size_t i = 0; i < vec.size(); i++) {
    output_c_server_hdr(vec[i]);
    vec[i]->isActiveUOC = false;
  }
}


static void
server_template_symdump(GCPtr<Symbol> s, INOstream& out,
			void (*f)(GCPtr<Symbol>, INOstream& out))
{
  switch(s->cls) {
  case sc_interface:
  case sc_absinterface:
    f(s, out);
    /* Fall Through */
  case sc_scope:
  case sc_package:
    {
      /* Walk recursively */
      for (size_t i = 0; i < s->children.size(); i++)
	server_template_symdump(s->children[i], out, f);

    break;
    }
  default:
    break;
  }
}

static void
emit_active_if_names(GCPtr<Symbol> s, INOstream& out)
{
  if (!s->isActiveUOC)
    return;

  out << s->QualifiedName('.') << "\n";
}

static void
emit_active_if_includes(GCPtr<Symbol> s, INOstream& out)
{
  if (!s->isActiveUOC)
    return;

  out << "#include <idl/" << s->QualifiedName('/') << ".server.h>\n";
}

static void
emit_active_if_legs(GCPtr<Symbol> s, INOstream& out)
{
  if (!s->isActiveUOC)
    return;

  out << "_IDL_IFUNION_" << s->QualifiedName('_') << "\n";
  out << "    " << s->QualifiedName('_') << ";\n";
}

static void
emit_active_if_switch_cases(GCPtr<Symbol> s, INOstream& out)
{
  if (!s->isActiveUOC)
    return;

  out << "case IKT_" << s->QualifiedName('_') << ":\n";
  out.more();
  out << "_IDL_IFDISPATCH_" << s->QualifiedName('_')
      << "(&gsu." << s->QualifiedName('_') << ", _env);\n"
      << "break;\n";
  out.less();
}

void
output_c_template(GCPtr<Symbol> globalScope, BackEndFn fn)
{
  if (outputFileName == Path())
    outputFileName = Path("template.c");

  std::ofstream outputFileStream(outputFileName.c_str(),
				 std::ios_base::out|std::ios_base::trunc);

  if (!outputFileStream.is_open()) {
    fprintf(stderr, "Couldn't open stub header file \"%s\"\n",
	    outputFileName.c_str());
    exit(1);
  }

  INOstream out(outputFileStream);

  out << "/* Example template for processing the following interfaces:\n";
  out.more();
  out.more();

  server_template_symdump(globalScope, out, emit_active_if_names);

  out.less();
  out.less();
  out << " */\n";

  out << "\n";
  out << "#include <coyotos/capidl.h>\n";
  out << "#include <coyotos/syscall.h>\n";
  out << "#include <coyotos/runtime.h>\n";
  out << "\n";

  server_template_symdump(globalScope, out, emit_active_if_includes);
  out << "\n";

  out << "typedef union {\n";
  out.more();
  server_template_symdump(globalScope, out, emit_active_if_legs);
  out << "InvParameterBlock_t pb;\n";
  out << "InvExceptionParameterBlock_t except;\n";
  out << "uintptr_t icw;\n";
  out.less();
  out << "} _IDL_GRAND_SERVER_UNION;\n";

  out << "\n"
      << "/* You should supply a function that selects an interface\n"
      << " * type based on the incoming endpoint ID and protected\n"
      << " * payload */\n"
      << "extern uint64_t choose_if(uint64_t epID, uint32_t pp);\n"
      << "\n";

  out << "/* The IDL_SERVER_Environment structure type is something\n"
      << " * that you should extend to hold any \"extra\" information\n"
      << " * you need to carry around in your handlers. CapIDL code\n"
      << " * will pass this pointer along, but knows absolutely\n"
      << " * nothing about the contents of the structure.\n"
      << " *\n"
      << " * The following structure definition is provided as a\n"
      << " * starting point. It is generally a good idea to pass the\n"
      << " * received protected payload and endpoint ID to handler\n"
      << " * functions so that they know what object was invoked and\n"
      << " * with what permissions.\n"
      << " */\n"
      << "typedef struct IDL_SERVER_Environment {\n";
  out.more();
  out << "uint32_t pp;\n"
      << "uint64_t epID;\n";
  out.less();
  out << "} IDL_SERVER_Environment;\n"
      << "\n"
      << "void\n"
      << "ProcessRequests(struct IDL_SERVER_Environment *_env)\n"
      << "{\n";
  out.more();
  out << "_IDL_GRAND_SERVER_UNION gsu;\n"
      << "\n"
      << "gsu.icw = 0;\n"
      << "gsu.pb.sndPtr = 0;\n"
      << "gsu.pb.sndLen = 0;\n"
      << "\n"
      << "for(;;) {\n";
  out.more();
  out << "gsu.icw &= (IPW0_LDW_MASK|IPW0_LSC_MASK\n"
      << "    |IPW0_SG|IPW0_SP|IPW0_SC|IPW0_EX);\n"
      << "gsu.icw |= IPW0_MAKE_NR(sc_InvokeCap)|IPW0_RP|IPW0_AC\n"
      << "    |IPW0_MAKE_LRC(3)|IPW0_NB|IPW0_CO;\n"
      << "\n"
      << "gsu.pb.u.invCap = CR_RETURN;\n"
      << "gsu.pb.rcvCap[0] = CR_RETURN;\n"
      << "gsu.pb.rcvCap[1] = CR_ARG0;\n"
      << "gsu.pb.rcvCap[2] = CR_ARG1;\n"
      << "gsu.pb.rcvCap[3] = CR_ARG2;\n"
      << "gsu.pb.rcvBound = (sizeof(gsu) - sizeof(gsu.pb));\n"
      << "gsu.pb.rcvPtr = ((char *)(&gsu)) + sizeof(gsu.pb);\n"
      << "\n"
      << "invoke_capability(&gsu.pb);\n"
      << "\n"
      << "_env->pp = gsu.pb.u.pp;\n"
      << "_env->epID = gsu.pb.epID;\n"
      << "\n"
      << "/* Re-establish defaults. Note we rely on the handler proc\n"
      << " * to decide how MANY of these caps will be sent by setting ICW.SC\n"
      << " * and ICW.lsc fields properly.\n"
      << " */\n"
      << "gsu.pb.sndCap[0] = CR_REPLY0;\n"
      << "gsu.pb.sndCap[1] = CR_REPLY1;\n"
      << "gsu.pb.sndCap[2] = CR_REPLY2;\n"
      << "gsu.pb.sndCap[3] = CR_REPLY3;\n"
      << "\n"
      << "/* We rely on the (de)marshaling procedures to set sndLen to zero\n"
      << " * if no string is to be sent. We cannot zero it preemptively here\n"
      << " * because sndLen is an IN parameter telling how many bytes we got.\n"
      << " * Set sndPtr to zero so that we will fault if this is mishandled.\n"
      << " */\n"
      << "gsu.pb.sndPtr = 0;\n"
      << "\n"
      << "if ((gsu.icw & IPW0_SC) == 0) {\n";
  out.more();
  out << "/* Protocol violation -- reply slot unpopulated. */\n"
      << "gsu.icw = 0;\n"
      << "gsu.pb.sndLen = 0;\n"
      << "continue;\n";
  out.less();
  out << "}\n";

  out << "\n"
      << "switch(choose_if(gsu.pb.epID, gsu.pb.u.pp)) {\n";

  server_template_symdump(globalScope, out, emit_active_if_switch_cases);

  out << "default:\n";
  {
    out.more();
    out << "{\n";
    out.more();
    out << "gsu.except.icw =\n"
	<< "  IPW0_MAKE_LDW((sizeof(gsu.except)/sizeof(uintptr_t))-1)\n"
	<< "  |IPW0_EX|IPW0_SP;\n"
	<< "gsu.except.exceptionCode = RC_coyotos_Cap_UnknownRequest;\n"
	<< "gsu.pb.sndLen = 0;\n"
	<< "break;\n";
    out.less();
    out << "}\n";
    out.less();
  }
  out << "}\n";
  out.less();
  out << "}\n";
  out.less();
  out << "}\n";
  // server_template_symdump(s, out);
}

static void
print_asmifdef(INOstream& out)
{
  size_t indent = out.indent_for_macro();
  out << "#ifndef __ASSEMBLER__\n";
  out.indent(indent);
}

static void
print_asmendif(INOstream& out)
{
  size_t indent = out.indent_for_macro();
  out << "#endif /* __ASSEMBLER__ */\n";
  out << "\n";
  out.indent(indent);
}
