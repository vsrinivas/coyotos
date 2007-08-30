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

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include <libsherpa/Path.hxx>
#include "SymTab.hxx"
#include "capidl.hxx"
#include "util.hxx"

static void print_asmifdef(std::ostream& out);
static void print_asmendif(std::ostream& out);

#if 0
std::string
c_serializer(GCPtr<Symbol> s)
{
  switch(s->cls) {
  case sc_primtype:
    {
      switch(s->v.lty){
      case lt_unsigned:
	switch(s->v.bn.as_uint32()) {
	case 0:
	  return "Integer";
	  break;
	case 8:
	  return "u8";
	  break;
	case 16:
	  return "u16";
	  break;
	case 32:
	  return "u32";
	  break;
	case 64:
	  return "u64";
	  break;
	default:
	  return "/* unknown integer size */";
	  break;
	};

	break;
      case lt_integer:
	switch(s->v.bn.as_uint32()) {
	case 0:
	  return "Integer";
	  break;
	case 8:
	  return "i8";
	  break;
	case 16:
	  return "i16";
	  break;
	case 32:
	  return "i32";
	  break;
	case 64:
	  return "i64";
	  break;
	default:
	  return "/* unknown integer size */";
	  break;
	};

	break;
      case lt_char:
	switch(s->v.bn.as_uint32()) {
	case 0:
	case 32:
	  return "wchar_t";
	  break;
	case 8:
	  return "char";
	  break;
	default:
	  return "/* unknown wchar size */";
	  break;
	}
	break;
      case lt_string:
	switch(s->v.bn.as_uint32()) {
	case 0:
	case 32:
	  return "wstring";
	  break;
	case 8:
	  return "string";
	  break;
	default:
	  return "/* unknown string size */";
	  break;
	}
	break;
      case lt_float:
	switch(s->v.bn.as_uint32()) {
	case 32:
	  return "f32";
	  break;
	case 64:
	  return "f64";
	  break;
	case 128:
	  return "f128";
	  break;
	default:
	  return "/* unknown float size */";
	  break;
	}
	break;
      case lt_bool:
	{
	  return "bool";
	  break;
	}
      case lt_void:
	{
	  return "void";
	  break;
	}
      default:
	return "/* Bad primtype type code */";
      }
    }
  case sc_symRef:
    {
      return c_serializer(s->value);
      break;
    }
  case sc_interface:
  case sc_absinterface:
    {
      return "cap_t";
    }
  default:
    return s->QualifiedName('_');
  }
}
#endif

/* c_byreftype is an optimization -- currently disabled, and needs
   debugging */
bool
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

BigNum
compute_value(GCPtr<Symbol> s)
{
  switch(s->cls) {
  case sc_const:
    {
      return compute_value(s->value);
    }
  case sc_symRef:
    {
      return compute_value(s->value);
    }

  case sc_value:
    {
      switch(s->cls) {
      case lt_integer:
      case lt_unsigned:
	return s->v.bn;

      default:
	{
	  fprintf(stderr,
		  "Constant computation with bad value type \"%s\"\n",
		  s->name.c_str());
	  exit(1);
	}
      }
      break;
    }
    case sc_arithop:
      {
	BigNum left;
	BigNum right;
	char op;

	assert(s->children.size());

	op = s->v.bn.as_uint32();
	assert (op == '-' || s->children.size() > 1);

	left = compute_value(s->children[0]);
	if (op == '-' && s->children.size() == 1) {
	  left = left.neg();
	  return left;
	}
	  
	right = compute_value(s->children[1]);

	switch(op) {
	  case '-':
	    {
	      left = left - right;
	      return left;
	    }
	  case '+':
	    {
	      left = left + right;
	      return left;
	    }
	  case '*':
	    {
	      left = left * right;
	      return left;
	    }
	  case '/':
	    {
	      left = left / right;
	      return left;
	    }
	  default:
	    {
	      fprintf(stderr, "Unhandled arithmetic operator \"%s\"\n",
		      s->name.c_str());
	      exit(1);
	    }
	}
      }
  default:
    {
      fprintf(stderr, "Busted arithmetic\n");
      exit(1);
    }
  }

  return 0;
}

void
output_c_type_trailer(GCPtr<Symbol> s, std::ostream& out, int indent)
{
  while (s->cls == sc_symRef)
    s = s->value;

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

/* NOTE: the 'indent' argument is NOT used here in the normal case. It
   is provided to indicate the indentation level at which the type is
   being output, so that types which require multiline output can be
   properly indented. The ONLY case in which do_indent() should be
   called from this routine is the multiline type output case. */
void
output_c_type(GCPtr<Symbol> s, std::ostream& out, int indent)
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

  if (s->IsVarSequenceType()) {
    s = s->type;
    out << "struct {\n";
    do_indent(out, indent+2);
    out << "unsigned long max;\n";
    do_indent(out, indent+2);
    out << "unsigned long len;\n";
    do_indent(out, indent+2);
    output_c_type(s, out, indent);
    out << " *data;\n";
    do_indent(out, indent);
    out << "}";

    return;
  }

  if (s->IsFixSequenceType())
    s = s->type;

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
      out << "cap_t";
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

static void
symdump(GCPtr<Symbol> s, std::ostream& out, int indent)
{
  if (s->docComment)
    PrintDocComment(s->docComment, out, indent);

  switch(s->cls){
  case sc_package:
    {
      /* Don't output nested packages. */
      if (s->nameSpace->cls == sc_package)
	return;

      do_indent(out, indent);
      out << "#ifndef __"
	  << s->QualifiedName('_')
	  << "__ /* package "
	  << s->QualifiedName('.')
	  << " */\n";
      out << "#define __"
	  << s->QualifiedName('_')
	  << "__\n";

      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out, indent + 2);

      out << "\n#endif /* __"
	  << s->QualifiedName('_')
	  <<"__ */\n";
      break;
    }
  case sc_scope:
    {
      out << "\n";
      do_indent(out, indent);
      out << "/* namespace "
	  << s->QualifiedName('_')
	  << " { */\n";

      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out, indent + 2);

      do_indent(out, indent);
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

      do_indent(out, indent);
      out << "typedef unsigned long " 
	  << s->QualifiedName('_')
	  << ";\n";

      print_asmendif(out);

      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out, indent + 2);
      break;
    }

  case sc_struct:
    {
      out << "\n";
      print_asmifdef(out);
      do_indent(out, indent);

      out << "typedef "
	  << s->ClassName()
	  << " "
	  << s->QualifiedName('_')
	  << " {\n";

      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out, indent + 2);

      do_indent(out, indent);
      out << "} "
	  << s->QualifiedName('_')
	  << ";\n";
      print_asmendif(out);
      break;
    }
  case sc_exception:
    {
      unsigned long sig = s->CodedName();

      out << "#define RC_"
	  << s->QualifiedName('_')
	  << " 0x"
	  << std::hex << sig << std::dec
	  << "\n";

      /* Exceptions only generate a struct definition if they 
	 actually have members. */
      if (s->children.size() > 0) {
	out << "\n";
	print_asmifdef(out);
	do_indent(out, indent);

	out << "struct "
	    << s->QualifiedName('_')
	    << " {\n";

	for(size_t i = 0; i < s->children.size(); i++)
	  symdump(s->children[i], out, indent + 2);

	do_indent(out, indent);
	out << "} ;\n";
	print_asmendif(out);
      }

      break;
    }

  case sc_interface:
  case sc_absinterface:
    {
      unsigned opr_ndx=1;	// opcode 0 is (arbitrarily) reserved

      unsigned long sig = s->CodedName();

      out << "\n";
      do_indent(out, indent);
      out << "/* BEGIN Interface "
	  << s->QualifiedName('.')
	  << " */\n";
      
      /* Identify the exceptions raised at the interface level first: */
      for(size_t i = 0; i < s->raises.size(); i++) {
	do_indent(out, indent+2);
	out << "/* interface raises "
	    << s->raises[i]->QualifiedName('_')
	    << " */\n";
      }

      out << "\n";

      out << "#define IKT_"
	  << s->QualifiedName('_')
	  << " 0x"
	  << std::hex << sig << std::dec
	  << "\n\n";

      do_indent(out, indent);
      print_asmifdef(out);
      out << "typedef cap_t "
	  << s->QualifiedName('_')
	  << ";\n";
      print_asmendif(out);
      out << "\n";

      for(size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];

	if (child->cls == sc_operation) {
	  if (child->flags & SF_NO_OPCODE) {
	    out << "\n/* Method "
		<< child->QualifiedName('_')
		<< " is client-only */\n";
	  }
	  else {
	    out << "\n#define OC_"
		<< child->QualifiedName('_')
		<< " 0x"
		<< std::hex << ((s->ifDepth << 24) | opr_ndx++) << std::dec
		<< "\n";
	  }
	}

	symdump(child, out, indent);
      }

      out << "\n";

      do_indent(out, indent);
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
      do_indent(out, indent);
      out << "typedef ";
      output_c_type(s->type, out, indent);
      out << ' ';
      out << s->QualifiedName('_');
      output_c_type_trailer(s->type, out, 0);
      out << ";\n";
      print_asmendif(out);
      break;
    }

  case sc_union:
  case sc_caseTag:
  case sc_caseScope:
  case sc_oneway:
  case sc_switch:
    {
      do_indent(out, indent);
      out << "/* cls "
	  << s->ClassName()
	  << " "
	  << s->QualifiedName('_')
	  << " */\n";

      break;
    }
  case sc_primtype:
    {
      do_indent(out, indent);
      output_c_type(s, out, indent);
      break;
    }
  case sc_operation:
    {
      assert(s->type->IsVoidType());

      print_asmifdef(out);
      do_indent(out, indent);
      out << "result_t";

      out << " "
	  << s->QualifiedName('_')
	  << "(cap_t _self";

      for(size_t i = 0; i < s->children.size(); i++) {
	out << ", ";
	symdump(s->children[i], out, 0);
      }

      out << ");\n";

      for(size_t i = 0; i < s->raises.size(); i++) {
	do_indent(out, indent + 2);
	out << "/* raises "
	    << s->raises[i]->QualifiedName('_')
	    << " */\n";
      }

      print_asmendif(out);
      break;
    }

  case sc_arithop:
    {
      do_indent(out, indent);
      out << '(';

      if (s->children.size() > 1) {
	symdump(s->children[0], out, indent + 2);
	out << s->name;
	symdump(s->children[1], out, indent + 2);
      }
      else {
	out <<s->name;
	symdump(s->children[0], out, indent + 2);
      }
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
      out << "#define "
	  << s->QualifiedName('_')
	  << " ";
      out << mi << "\n";
      break;
    }

  case sc_member:
    {
      do_indent(out, indent);
      output_c_type(s->type, out, indent);

      out << " " << s->name;

      output_c_type_trailer(s->type, out, 0);

      out << ";\n";

      break;
    }
  case sc_formal:
  case sc_outformal:
    {
      bool wantPtr = (s->cls == sc_outformal) || c_byreftype(s->type);
      wantPtr = wantPtr && !s->type->IsInterface();

      output_c_type(s->type, out, 0);
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
output_c_hdr(GCPtr<Symbol> s)
{
  Path fileName;
  Path dirName;
  SymVec vec;
  
  if (s->isActiveUOC == false)
    return;

  {
    std::string sqn = s->QualifiedName('/');
    fileName = (Path(target) + Path(sqn)) << ".h";
  }

  dirName = fileName.dirName();

  dirName.smkdir();

  std::ofstream out(fileName.c_str(), std::ios_base::out|std::ios_base::trunc);

  if (!out.is_open()) {
    fprintf(stderr, "Couldn't open stub header file \"%s\"\n",
	    fileName.c_str());
    exit(1);
  }

  s->ComputeDependencies(vec);

  vec.qsort(Symbol::CompareByQualifiedName);

  /* Header guards were missing.  I've added them here */
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
  print_asmifdef(out);
  out << "#include <coyotos/capidl.h>\n";
  print_asmendif(out);

  for (size_t i = 0; i < vec.size(); i++) {
    GCPtr<Symbol> sym = vec[i];

    out << "#include <idl/"
	<< sym->QualifiedName('/')
	<< ".h>\n";
  }

  symdump(s, out, 0);

  /* final endif for header guards */
  out << "\n";
  out << "#endif /* __"
      << s->QualifiedName('_')
      << "_h__ */\n";

  out.close();
}

static void
print_asmifdef(std::ostream& out)
{
  out << "#ifndef __ASSEMBLER__\n";
}

static void
print_asmendif(std::ostream& out)
{
  out << "#endif /* __ASSEMBLER__ */\n";
}
