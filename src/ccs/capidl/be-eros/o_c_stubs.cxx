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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include <libsherpa/Path.hxx>

#include "SymTab.hxx"
#include "capidl.hxx"
#include "util.hxx"
#include "backend.hxx"

/* Size of native register */
#define REGISTER_BITS      32

/* Size of largest integral type that we will attempt to registerize: */
#define MAX_REGISTERIZABLE 64

#define FIRST_REG 1		/* reserve 1 for opcode/result code */
#define MAX_REGS  4

extern bool c_byreftype(GCPtr<Symbol> s);
extern void output_c_type(GCPtr<Symbol> s, std::ostream& out, int indent);
extern void output_c_type_trailer(GCPtr<Symbol> s, std::ostream& out, int indent);
extern BigNum compute_value(GCPtr<Symbol> s);

/* can_registerize(): given a symbol /s/ denoting an argument, and a
   number /nReg/ indicating how many register slots are taken, return:

     0   if the symbol /s/ cannot be passed in registers, or
     N>0 the number of registers that the symbol /s/ will occupy.

   note that a 64 bit integer quantity will be registerized, but only
   if it can be squeezed into the number of registers that remain
   available.
*/
unsigned 
can_registerize(GCPtr<Symbol> s, unsigned nReg)
{
  s = s->ResolveType();
  
  assert(s->IsBasicType());

  if (nReg == MAX_REGS)
    return 0;

  while (s->cls == sc_symRef)
    s = s->value;

  if (s->cls == sc_typedef)
    return can_registerize(s->type, nReg);

  if ((s->cls == sc_enum) || (s->cls == sc_bitset))
    return 1;

  switch(s->cls) {
  case sc_primtype:
    {
      switch(s->v.lty) {
      case lt_integer:
      case lt_unsigned:
      case lt_char:
      case lt_bool:
	{
	  /* Integral types are aligned to their size. Floating types
	     are aligned to their size, not to exceed 64 bits. If we
	     ever support larger fixed integers, we'll need to break
	     these cases up. */

	  unsigned bits = s->v.bn.as_uint32();
      
	  if (bits == 0)
	    return 0;

	  if (nReg >= MAX_REGS)
	    return 0;

	  if (bits <= REGISTER_BITS)
	    return 1;

	  if (bits <= MAX_REGISTERIZABLE) {
	    unsigned needRegs = bits / REGISTER_BITS;
	    if (nReg + needRegs <= MAX_REGS)
	      return needRegs;
	  }

	  return 0;
	}
      default:
	return 0;
      }
    }
  case sc_enum:
  case sc_bitset:
    {
      /* Note assumption that enumeral type fits in a register */
      if (nReg >= MAX_REGS)
	return 0;

      return 1;
    }

  default:
    return 0;
 }
}

void
emit_registerize(std::ostream& out, GCPtr<Symbol> child, int indent, unsigned regCount)
{
  unsigned bitsOutput = 0;
  unsigned bits = child->type->v.bn.as_uint32();

  {
    do_indent(out, indent);
    out << "msg.snd_w" << regCount << " = " << child->name << "\n;";

    bitsOutput += REGISTER_BITS;
    regCount++;
  }

  /* If the quantity is larger than a single register, we need to
     output the rest of it: */

  while (bitsOutput < bits) {
    do_indent(out, indent);
    out << "msg.snd_w" << regCount << " = (" 
	<< child->name << ">> " << bitsOutput << ");\n";

    bitsOutput += REGISTER_BITS;
    regCount++;
  }
}

void
emit_deregisterize(std::ostream& out, GCPtr<Symbol> child, int indent, unsigned regCount)
{
  unsigned bitsInput = 0;
  unsigned bits = child->type->v.bn.as_uint32();

  {
    do_indent(out, indent);
    out << "if ("
	<< child->name
	<< ") *"
	<< child->name
	<< " = msg.rcv_w"
	<< regCount
	<< ";\n";

    bitsInput += REGISTER_BITS;
    regCount++;
  }

  /* If the quantity is larger than a single register, we need to
     decode the rest of it: */

  while (bitsInput < bits) {
    do_indent(out, indent);
    out << "if ("
	<< child->name
	<< ") *"
	<< child->name
	<< " |= ( ((";

    output_c_type(child->type, out, 0);

    out << ")msg.rcv_w"
	<< regCount
	<< ") << "
	<< bitsInput
	<< " );\n";

    bitsInput += REGISTER_BITS;
    regCount++;
  }
}

SymVec
extract_registerizable_arguments(GCPtr<Symbol> s, SymClass sc)
{
  unsigned nReg = FIRST_REG;
  unsigned needRegs;
  SymVec regArgs;

  for(size_t i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> child = s->children[i];

    if (child->cls != sc)
      continue;

    if (child->type->IsInterface()) {
      regArgs.append(child);
      continue;
    }

    if ((needRegs = can_registerize(child->type, nReg))) {
      regArgs.append(child);
      nReg += needRegs;
      continue;
    }
  }

  return regArgs;
}

SymVec
extract_string_arguments(GCPtr<Symbol> s, SymClass sc)
{
  unsigned nReg = FIRST_REG;
  unsigned needRegs;
  SymVec stringArgs;

  for(size_t i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> child = s->children[i];

    if (child->cls != sc)
      continue;

    if (child->type->IsInterface())
      continue;

    if ((needRegs = can_registerize(child->type, nReg))) {
      nReg += needRegs;
      continue;
    }

stringArgs.append(child);
  }

  return stringArgs;
}

unsigned
compute_indirect_bytes(SymVec& symVec)
{
  unsigned sz = 0;

  for(size_t i = 0; i < symVec.size(); i++) {
    GCPtr<Symbol> sym = symVec[i];
    sz = round_up(sz, sym->alignof());
    sz += sym->indirectSize();
  }

  return sz;
}

unsigned
compute_direct_bytes(SymVec& symVec)
{
  unsigned sz = 0;

  for(size_t i = 0; i < symVec.size(); i++) {
    GCPtr<Symbol> sym = symVec[i];
    sz = round_up(sz, sym->alignof());
    sz += sym->directSize();
  }

  return sz;
}

unsigned
emit_symbol_align(const std::string& lenVar, std::ostream& out, int indent,
		  unsigned elemAlign, unsigned align)
{
  if ((elemAlign & align) == 0) {
    do_indent(out, indent);
    out << lenVar
	<< " += ("
	<< elemAlign
	<< "-1); "
	<< lenVar
	<< " -= ("
	<< lenVar
	<< " % "
	<< elemAlign
	<< ");\t/* align to "
	<< elemAlign
	<< " */\n";
  }

  return (elemAlign * 2) - 1;
}
     
/* The /align/ field is a bitmap representing which power of two
   alignments are known to be valid. */
unsigned
emit_direct_symbol_size(const std::string& fullName, GCPtr<Symbol> s, 
			std::ostream& out, int indent, SymClass sc, 
			unsigned align)
{
  std::string lenVar = (sc == sc_formal) ? "sndLen" : "rcvLen";
      
  switch(s->cls) {
  case sc_formal:
  case sc_outformal:
  case sc_typedef:
    {
      return emit_direct_symbol_size(fullName, s->type, out,
				     indent, sc, align);
    }
  case sc_member:
    {
      return emit_direct_symbol_size(symname_join(fullName, s->name, '.'),
				     s->type, out, indent, sc, align);
    }
  case sc_symRef:
    {
      return emit_direct_symbol_size(fullName, s->value, out, indent, sc,
				     align);
    }
    
  case sc_primtype:
  case sc_enum:
  case sc_bitset:
  case sc_struct:
  case sc_seqType:
  case sc_bufType:
  case sc_arrayType:
    {
      /* No indirect size */
      unsigned elemAlign = s->alignof();
      unsigned elemSize = s->directSize();
      elemSize = round_up(elemSize, elemAlign);

      align = emit_symbol_align(lenVar, out, indent, elemAlign, align);

      do_indent(out, indent);
      out << lenVar
	  << " += "
	  << elemSize
	  << ";\t/* sizeof("
	  << fullName
	  << "), align="
	  << elemAlign
	  << " */\n";

      return align;
    }

  case sc_union:
    {
      assert(false);
      return align;
    }
  case sc_interface:
  case sc_absinterface:
    {
      assert(false);
      return align;
    }
  default:
    {
      fprintf(stderr, "Size computation of unknown type for symbol \"%s\"\n", 
	      s->QualifiedName('.').c_str());
      exit(1);
      /*      return align; */
    }
  }

  return align;
}

/* The /align/ field is a bitmap representing which power of two
   alignments are known to be valid. */
unsigned
emit_indirect_symbol_size(const std::string& fullName, GCPtr<Symbol> s, 
			  std::ostream& out, int indent, SymClass sc, unsigned align)
{
  std::string lenVar = (sc == sc_formal) ? "sndLen" : "rcvLen";
      
  switch(s->cls) {
  case sc_formal:
  case sc_outformal:
  case sc_typedef:
    {
      return emit_indirect_symbol_size(fullName, s->type, out,
				       indent, sc, align);
    }
  case sc_member:
    {
      return emit_indirect_symbol_size(symname_join(fullName, s->name, '.'),
				       s->type, out, indent, sc, align);
    }
  case sc_symRef:
    {
      return emit_indirect_symbol_size(fullName, s->value, out, indent, sc,
				       align);
    }
    
  case sc_primtype:
  case sc_enum:
  case sc_bitset:
    /* No indirect size */
    return align;

  case sc_struct:
    {
      /* Compute indirect size on each member in turn */
      for(size_t i = 0; i < s->children.size(); i++) {
	GCPtr<Symbol> child = s->children[i];
	align = emit_indirect_symbol_size(fullName, child, out, indent,
					  sc, align);
      }

      return align;
    }
  case sc_seqType:
  case sc_bufType:
    {
      unsigned elemAlign = s->type->alignof();
      unsigned elemSize = s->type->directSize();
      elemSize = round_up(elemSize, elemAlign);

      /* See if an alignment adjustment is needed: */
      align = emit_symbol_align(lenVar, out, indent, elemAlign, align);
      
      do_indent(out, indent);
      if (sc == sc_formal) {
	out << lenVar
	    << " += "
	    << elemSize
	    << " * "
	    << fullName
	    << ".len; /* "
	    << fullName
	    << " vector content, align="
	    << elemAlign
	    << " */\n";
      }
      else {
	GCPtr<Symbol> theSeqType = s->ResolveType();
	BigNum bound = compute_value(theSeqType->value);
	out << lenVar 
	    << " += "
	    << elemSize
	    << " * "
	    << bound
	    << "; /* "
	    << fullName
	    << " vector MAX content, align="
	    << elemAlign
	    << " */\n";
      }
      
      return (2 * elemAlign) - 1;
    }
  case sc_arrayType:
    {
      /* Array type is always inline, never indirect. */
      return align;
    }
  case sc_union:
    {
      assert(false);
      return align;
    }
  case sc_interface:
  case sc_absinterface:
    {
      assert(false);
      return align;
    }
  default:
    {
      fprintf(stderr, "Size computation of unknown type for symbol \"%s\"\n", 
	      s->QualifiedName('.').c_str());
      exit(1);
      /* return align; */
    }
  }

  return align;
}

unsigned
emit_direct_byte_computation(SymVec& symVec, std::ostream& out, int indent,
			     SymClass sc, unsigned align)
{
  for(size_t i = 0; i < symVec.size(); i++) {
    GCPtr<Symbol> sym = symVec[i];
    align = emit_direct_symbol_size(sym->name, sym, out, indent, sc, align);
  }

  return align;
}

unsigned
emit_indirect_byte_computation(SymVec& symVec, std::ostream& out, int indent,
			       SymClass sc, unsigned align)
{
  for(size_t i = 0; i < symVec.size(); i++) {
    GCPtr<Symbol> sym = symVec[i];
    align = emit_indirect_symbol_size(sym->name, sym, out, indent, sc, align);
  }

  return align;
}

void
emit_send_string(SymVec& symVec, std::ostream& out, int indent)
{
  if (symVec.size() == 0)
    return;

  /* Choose strategy: */
  if (symVec.size() == 1 &&
      symVec[0]->type->IsDirectSerializable()) {
    GCPtr<Symbol> s0 = symVec[0];

    do_indent(out, indent);
    out << "/* Using direct method */\n";

    if (s0->type->IsVarSequenceType()) {
      do_indent(out, indent);
      out << "msg.snd_len = "
	  << s0->name
	  << "_len * sizeof(";
      output_c_type(s0->type, out, 0);
      out << ");\n";
    }
    else if (s0->type->IsFixSequenceType()) {
      do_indent(out, indent);
      out << "msg.snd_len = sizeof("
	  << s0->name
	  << ");\n";
    }
    else {
      do_indent(out, indent);
      out << "msg.snd_len = sizeof("
	  << (c_byreftype(s0->type) ? "*" : "")
	  << s0->name
	  << ");\n";
    }
    do_indent(out, indent);
    out << "msg.snd_data = "
	<< (c_byreftype(s0->type) ? "" : "&")
	<< s0->name
	<< ";\n";
    out << "\n";
  }
  else {
    unsigned align = 0xfu;
    unsigned indirAlign = 0xfu;

    do_indent(out, indent);
    out << "msg.snd_data = sndData;\n";
    do_indent(out, indent);
    out << "msg.snd_len = sndLen;\n";
    out << "\n";
    do_indent(out, indent);
    out << "sndLen = 0;\n";

    for(size_t i = 0; i < symVec.size(); i++) {
      GCPtr<Symbol> arg = symVec[i];
      GCPtr<Symbol> argType = arg->type->ResolveRef();
      GCPtr<Symbol> argBaseType = arg->type->ResolveType();
      
      out << "\n";
      do_indent(out, indent);
      out <<"/* Serialize "
	  << arg->name
	  << " */\n";
      do_indent(out, indent);
      out << "{\n";
      do_indent(out, indent+2);

      output_c_type(argType, out, 0);
      out << " *_CAPIDL_arg;\n\n";

      align = emit_symbol_align("sndLen", out, indent+2,
				argBaseType->alignof(), align);

      do_indent(out, indent+2);
      out << "_CAPIDL_arg = (";
      output_c_type(argType, out, 0);
      out << " *) (sndData + sndLen);\n";

      if (argBaseType->IsFixSequenceType()) {
	BigNum bound = compute_value(argBaseType->value);

	do_indent(out, indent+2);
	out << "__builtin_memcpy(_CAPIDL_arg, "
	    << arg->name
	    << ", "
	    << bound
	    << " * sizeof(*"
	    << arg->name
	    << "));\n";
      }
      else {
	do_indent(out, indent+2);
	out << "*_CAPIDL_arg = "
	    << arg->name
	    << ";\n";
	do_indent(out, indent+2);
	out << "sndLen += sizeof("
	    << arg->name
	    << ");\n";

	if (argBaseType->IsVarSequenceType()) {
	  do_indent(out, indent+2);

	  indirAlign = emit_symbol_align("sndIndir", out, indent+2,
				    argBaseType->alignof(), indirAlign);

	  out << "_CAPIDL_arg->data = (";
	  output_c_type(argBaseType->type, out, 0);
	  out << " *) (sndData + sndIndir);\n";

	  do_indent(out, indent+2);
	  out << "__builtin_memcpy(_CAPIDL_arg->data, "
	      << arg->name 
	      << ".data, sizeof(*"
	      << arg->name
	      << ".data) * "
	      << arg->name
	      << ".len);\n";

	  out << "\n";
	  do_indent(out, indent+2);
#if 0
	  fprintf(out, "/* Encode the vector offset for transmission */\n");
	  do_indent(out, indent+2);
	  fprintf(out, "((unsigned long) _CAPIDL_arg->data) = sndIndir;\n\n");

	  do_indent(out, indent+2);
#endif
	  out << "sndIndir += sizeof("
	      << arg->name
	      << ".data) * "
	      << arg->name
	      << ".len;\n";
	}
	  
      }
      do_indent(out, indent);
      out << "}\n";
      
    }
  }
}

static void
emit_receive_string(SymVec& symVec, std::ostream& out, int indent)
{
  if (symVec.size() == 0)
    return;
  
  /* Choose strategy: */
  if (symVec.size() == 1 &&
      symVec[0]->type->IsDirectSerializable()) {
    GCPtr<Symbol> s0 = symVec[0];

    do_indent(out, indent);
    out << "/* Using direct method */\n";

    do_indent(out, indent);
    out << "msg.rcv_limit = sizeof(*"
	<< s0->name
	<< ");\n";
    do_indent(out, indent);
    out << "msg.rcv_data = "
	<< s0->name
	<< ";\n"; /* already set up as a pointer */
    out << "\n";
  }
  else {
    do_indent(out, indent);
    out << "msg.rcv_data = rcvData;\n";
    do_indent(out, indent);
    out << "msg.rcv_limit = rcvLen;\n";
  }
}

static void
emit_unpack_return_registers(SymVec& symVec, std::ostream& out, int indent)
{
  unsigned nReg = FIRST_REG;
  unsigned needRegs;

  for(size_t i = 0; i < symVec.size(); i++) {
    GCPtr<Symbol> child = symVec[i];

    if (child->cls != sc_outformal)
      continue;

    if (child->type->IsInterface())
      continue;

    if ((needRegs = can_registerize(child->type, nReg))) {
      emit_deregisterize(out, child, indent+2, nReg);
      nReg += needRegs;
      continue;
    }
  }
}

static void
emit_unpack_return_string(SymVec& symVec, std::ostream& out, int indent)
{
  bool isDirect;
  unsigned align = 0xfu;
  unsigned indirAlign = 0xfu;

  /* Choose strategy: */
  isDirect = (symVec.size() == 1 &&
	      symVec[0]->type->IsDirectSerializable());

  if (isDirect)
    return;

  do_indent(out, indent);
  out << "rcvLen = 0;\n";

  for(size_t i = 0; i < symVec.size(); i++) {
    GCPtr<Symbol> arg = symVec[i];
    GCPtr<Symbol> argType = arg->type->ResolveRef();
    GCPtr<Symbol> argBaseType = arg->type->ResolveType();
      
    out << "\n";
    do_indent(out, indent);
    out << "/* Deserialize "
	<< arg->name
	<< " */\n";
    do_indent(out, indent);
    out << "{\n";
    do_indent(out, indent+2);

    output_c_type(argType, out, 0);
    out << " *_CAPIDL_arg;\n\n";

    align = emit_symbol_align("rcvLen", out, indent+2,
			      argBaseType->alignof(), align);

    do_indent(out, indent+2);
    out << "_CAPIDL_arg = (";
    output_c_type(argType, out, 0);
    out << " *) (rcvData + rcvLen);\n";

    if (argBaseType->IsFixSequenceType()) {
      BigNum bound = compute_value(argBaseType->value);

      do_indent(out, indent+2);
      out << "__builtin_memcpy("
	  << arg->name
	  << ", rcvData + rcvLen, "
	  << bound
	  << " * sizeof(*"
	  << arg->name
	  << "));\n";

      do_indent(out, indent+2);
      out << "rcvLen += ("
	  << bound
	  << " * sizeof(*"
	  << arg->name
	  << "));\n";
    }
    else {
      do_indent(out, indent+2);
      out << "*"
	  << arg->name
	  << " = *_CAPIDL_arg;\n";
      do_indent(out, indent+2);
      out << "rcvLen += sizeof(*"
	  << arg->name
	  << ");\n";

      if (argBaseType->IsVarSequenceType()) {
	BigNum bound = compute_value(argBaseType->value);
	unsigned int ubound = bound.as_uint32();
	
	do_indent(out, indent+2);

	indirAlign = emit_symbol_align("rcvIndir", out, indent+2,
				       argBaseType->alignof(), indirAlign);

	out << "_CAPIDL_arg->data = (";
	output_c_type(argBaseType->type, out, 0);
	out << " *) (rcvData + rcvIndir);\n";

	do_indent(out, indent+2);
	out << "__builtin_memcpy(" 
	    << arg->name
	    << "->data, _CAPIDL_arg->data, "
	    << "sizeof("
	    << arg->name
	    << "->data) * "
	    << "(_CAPIDL_arg->len < " 
	    << ubound
	    << " ? _CAPIDL_arg->len : "
	    << ubound
	    << "));\n";

	out << "\n";

	do_indent(out, indent+2);
	out << "rcvIndir += sizeof(" 
	    << arg->name
	    << "->data) * "
	    << "(_CAPIDL_arg->len < " 
	    << ubound
	    << " ? _CAPIDL_arg->len : "
	    << ubound
	    << ");\n";
      }
	  
    }
    do_indent(out, indent);
    out << "}\n";
      
  }
}

#if 0
bool
c_excpt_needs_message_string(GCPtr<Symbol> ex)
{
  assert(ex->cls == sc_exception);

  /* Exception structures are ALWAYS passed as stringified responses. */
  return vec_len(ex->children) != 0;
}
#endif

#if 0
bool
c_if_needs_exception_string(GCPtr<Symbol> s)
{
  if (s->baseType) {
    GCPtr<Symbol> base = s->baseType;
    while (base->cls == sc_symRef)
      base = base->value;

    if (c_if_needs_exception_string(base))
      return true;
  }

  for(i = 0; i < vec_len(s->raises); i++) {
    GCPtr<Symbol> ex = symvec_fetch(s->raises,i);
    while(ex->cls == sc_symRef)
      ex = ex->value;

    if (c_excpt_needs_message_string(ex))
      return true;
  }

  return false;
}
#endif

#if 0
bool
c_op_needs_exception_string(GCPtr<Symbol> op)
{
  unsigned a;
  for (a = 0; a < vec_len(op->raises); a++) {
    GCPtr<Symbol> ex = symvec_fetch(op->raises,a);
    while(ex->cls == sc_symRef)
      ex = ex->value;

    if (c_excpt_needs_message_string(ex))
      return true;
  }

  if (c_if_needs_exception_string(op->nameSpace))
    return true;

  return false;
}
#endif

/* c_op_needs_message_string(): returns
 *
 *     0 if no message string is required
 *     1 if a message string is needed and a struct must be built
 *     2 if a message string is needed but can be serialized directly.
 */
static unsigned
c_op_needs_message_string(GCPtr<Symbol> op, SymClass sc)
{
  unsigned nReg = FIRST_REG;
  unsigned needRegs;

  unsigned nStringElem = 0;
  unsigned nDirectElem = 0;

  for (size_t a = 0; a < op->children.size(); a++) {
    GCPtr<Symbol> arg = op->children[a];

    if (arg->cls != sc)
      continue;

    if (arg->type->IsInterface()) {
    }
    else if ((needRegs = can_registerize(arg->type, nReg))) {
      nReg += needRegs;
    }
    else {
      nStringElem ++;
      if (arg->type->IsDirectSerializable())
	nDirectElem ++;
    }
  }

  if (nStringElem == 1 && nDirectElem == 1)
    return 2;
  else 
    return (nStringElem ? 1 : 0);
}

#if 0
bool
c_if_needs_message_string(GCPtr<Symbol> s, SymClass sc)
{
  if (s->baseType) {
    GCPtr<Symbol> base = s->baseType;
    while (base->cls == sc_symRef)
      base = base->value;

    if (c_if_needs_message_string(base, sc))
      return true;
  }

  for(i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> op = symvec_fetch(s->children,i);
    if (op->cls != sc_operation)
      continue;

    if (c_op_needs_message_string(op, sc))
      return true;
  }

  return false;
}
#endif

static void
output_client_stub(std::ostream& out, const std::string& preamble, GCPtr<Symbol> s, int indent)
{
  SymVec sndRegs = extract_registerizable_arguments(s, sc_formal);
  SymVec rcvRegs = extract_registerizable_arguments(s, sc_outformal);
  SymVec sndString = extract_string_arguments(s, sc_formal);
  SymVec rcvString = extract_string_arguments(s, sc_outformal);

  unsigned snd_capcount = 0;
  unsigned rcv_capcount = 0;
  unsigned snd_regcount = FIRST_REG;
  unsigned rcv_regcount = FIRST_REG;

  unsigned needSendString = c_op_needs_message_string(s, sc_formal);
  unsigned needRcvString = c_op_needs_message_string(s, sc_outformal);

  unsigned needRegs;

  out << preamble;

  /***************************************************************
   * Function return value, name, and parameters
   ***************************************************************/
  assert(s->type->IsVoidType());
  out << "result_t";

  /* Function prefix and arguments */
  out << "\n" 
      << s->QualifiedName('_')
      << "(cap_t _self";

  for(size_t i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> child = s->children[i];

    bool wantPtr = (child->cls == sc_outformal) || c_byreftype(child->type);
    wantPtr = wantPtr && !child->type->IsInterface();

    out << ", ";
    output_c_type(child->type, out, 0);
    out << " ";
    if (wantPtr)
      out << "*";
    out << child->name;
    /* Normally would call output_c_type_trailer, but that is only
       used to add the trailing "[size]" to vectors, and we don't want
       that in the procedure signature. 

       output_c_type_trailer(child->type, out, 0);
    */
  }

  out << ")\n{\n";

  /***************************************************************
   * Function body.
   ***************************************************************/
    
  /* Setup generic message structure. Some of this will be overwritten 
     below, but the compiler's CSE/DCE optimizations will eliminate
     the redundancy. */
  do_indent(out, indent + 2);
  out << "Message msg;\n\n";

  if (needSendString == 1) {
    do_indent(out, indent + 2);
    out << "unsigned char *sndData;\n";
    do_indent(out, indent + 2);
    out << "unsigned sndLen = 0;\n";
    do_indent(out, indent + 2);
    out << "unsigned sndIndir = 0;\n";
  }
  if (needRcvString == 1) {
    do_indent(out, indent + 2);
    out << "unsigned char *rcvData;\n";
    do_indent(out, indent + 2);
    out << "unsigned rcvLen = 0;\n";
    do_indent(out, indent + 2);
    out << "unsigned rcvIndir = 0;\n";
  }

  if (needSendString == 1) {
    unsigned align = 0xfu;	/* alloca returns word-aligned storage */
    out << "\n";
    do_indent(out, indent + 2);
    out << "/* send string size computation */\n";
    align = emit_direct_byte_computation(sndString, out, indent+2,
					 sc_formal, align);
    /* Align up to an 8 byte boundary to begin the indirect bytes */
    align = emit_symbol_align("sndLen", out, indent+2, 8, align);

    do_indent(out, indent + 2);
    out << "sndIndir = sndLen;\n";
    align = emit_indirect_byte_computation(sndString, out, indent+2,
					   sc_formal, align);
    do_indent(out, indent + 2);
    out << "sndData = alloca(sndLen);\n";
  }
  if (needRcvString == 1) {
    unsigned align = 0xfu;	/* alloca returns word-aligned storage */
    out << "\n";
    do_indent(out, indent + 2);
    out << "/* receive string size computation */\n";
    align = emit_direct_byte_computation(rcvString, out, indent+2,
					 sc_outformal, align);

    /* Align up to an 8 byte boundary to begin the indirect bytes */
    align = emit_symbol_align("rcvLen", out, indent+2, 8, align);

    do_indent(out, indent + 2);
    out << "rcvIndir = rcvLen;\n";

    emit_indirect_byte_computation(rcvString, out, indent+2,
				   sc_outformal, align);
    do_indent(out, indent + 2);
    out << "rcvData = alloca(rcvLen);\n";
  }
    
  out << "\n";

  do_indent(out, indent + 2);
  out << "msg.snd_invKey = _self;\n";
  do_indent(out, indent + 2);
  out << "msg.snd_code = OC_" 
      << s->QualifiedName('_')
      << ";\n";
  do_indent(out, indent + 2);
  out << "msg.snd_w1 = 0;\n";
  do_indent(out, indent + 2);
  out << "msg.snd_w2 = 0;\n";
  do_indent(out, indent + 2);
  out << "msg.snd_w3 = 0;\n";
  do_indent(out, indent + 2);
  out << "msg.snd_len = 0;\n";
  do_indent(out, indent + 2);
  out << "msg.snd_key0 = KR_VOID;\n";
  do_indent(out, indent + 2);
  out << "msg.snd_key1 = KR_VOID;\n";
  do_indent(out, indent + 2);
  out << "msg.snd_key2 = KR_VOID;\n";
  do_indent(out, indent + 2);
  out << "msg.snd_rsmkey = KR_VOID;\n";

  out << "\n";

  do_indent(out, indent + 2);
  out << "msg.rcv_limit = 0;\n";
  do_indent(out, indent + 2);
  out << "msg.rcv_key0 = KR_VOID;\n";
  do_indent(out, indent + 2);
  out << "msg.rcv_key1 = KR_VOID;\n";
  do_indent(out, indent + 2);
  out << "msg.rcv_key2 = KR_VOID;\n";
  do_indent(out, indent + 2);
  out << "msg.rcv_rsmkey = KR_VOID;\n";

  out << "\n";

  /* Reserve slot for the return type first, if it is registerizable: */
  rcv_regcount += can_registerize(s->type, rcv_regcount);

  /* Sent registerizable arguments */
  for(size_t i = 0; i < sndRegs.size(); i++) {
    GCPtr<Symbol> child = sndRegs[i];

    if (child->type->IsInterface()) {
      if (snd_capcount >= 3) {
	fprintf(stderr, "Too many capabilities transmitted\n");
	exit(1);
      }

      do_indent(out, indent + 2);
      out << "msg.snd_key" 
	  << snd_capcount++
	  << " = "
	  << child->name
	  << ";\n";
    }
    else if ((needRegs = can_registerize(child->type, snd_regcount))) {
      emit_registerize(out, child, indent + 2, snd_regcount);
      snd_regcount += needRegs;
    }
  }

  /* Received registerizable arguments */
  for(size_t i = 0; i < rcvRegs.size(); i++) {
    GCPtr<Symbol> child = rcvRegs[i];

    if (child->type->IsInterface()) {
      if (rcv_capcount >= 3) {
	fprintf(stderr, "Too many capabilities received\n");
	exit(1);
      }

      do_indent(out, indent + 2);
      out << "msg.rcv_key" 
	  << rcv_capcount++
	  << " = "
	  << child->name
	  << ";\n";
    }
    else if ((needRegs = can_registerize(child->type, rcv_regcount))) {
      /* do nothing -- handled through registerization below */
      rcv_regcount += needRegs;
    }
  }

  /* If the return type is a capability type, convert it to an out
     parameter at the end of the parameter list */
  if (s->type->IsInterface()) {
    if (rcv_capcount >= 3) {
      fprintf(stderr, "Too many capabilities received\n");
      exit(1);
    }

    do_indent(out, indent + 2);
    out << "msg.rcv_key" 
	<< rcv_capcount++
	<< " = _resultCap;\n";
  }

  emit_send_string(sndString, out, indent+2);

  emit_receive_string(rcvString, out, indent+2);

  if (needSendString || needRcvString)
    out << "\n";

  do_indent(out, indent + 2);
  out << "CALL(&msg);\n";
  out << "\n";

  rcv_regcount = 1;

  do_indent(out, indent+2);
  out << "if (msg.rcv_code != RC_OK) return msg.rcv_code;\n";

  /* Unpack the registers and the structure (if any) that contain the
     returned arguments (if any) */
  emit_unpack_return_registers(rcvRegs, out, indent);
  if (needRcvString)
    emit_unpack_return_string(rcvString, out, indent + 2);

  do_indent(out, indent + 2);
  out << "return msg.rcv_code;\n";

  out << "}\n";
}

#if 0
static bool
output_server_message_strings(std::ostream& out, GCPtr<Symbol> s, SymClass sc, int indent)
{
  bool haveString = false;

  if (s->baseType) {
    GCPtr<Symbol> base = s->baseType;
    while (base->cls == sc_symRef)
      base = base->value;

    output_server_message_strings(out, base, sc, indent);
  }

  for(i = 0; i < s->children.size(); i++) {
    InternedString nm;
   GCPtr<Symbol> op = s->children[i];
    if (op->cls != sc_operation)
      continue;

    {
      char *tmp = VMALLOC(char, strlen("OP_") + strlen(op->name) + 1);
      strcpy(tmp, "OP_");
      strcat(tmp, op->name);

      nm = intern(tmp);
      free(tmp);
    }

    if (setup_message_string(op, out, sc, indent, 
			     nm, false))
      haveString = true;
  }

  return haveString;
}
#endif

#if 0
/** This is the dual of the client-side stub generation, but somewhat
    more complicated by the need to pre-allocate the storage earlier
    in the procedure and perform all of the necessary casts.  Input
    symbol /op/ is the sc_operation symbol. */
static void
output_server_dispatch_args(std::ostream& out, GCPtr<Symbol> op)
{
  unsigned nRcvReg = 1;
  unsigned nRcvCap = 1;
  unsigned nSndReg = 1;
  unsigned nSndCap = 1;

  char *comma = "";

  for(unsigned i = 0; i < op->children.size(); i++) {
    GCPtr<Symbol> arg = op->children[i];

    out << comma;

    if (arg->cls == sc_formal) {
      if (symbol_IsInterface(arg->type))
	out << "msg->rcv_key" << nRcvCap++;
      else if (can_registerize(arg->type) && nRcvReg < MAX_REGS) {
	out << '(';
	output_c_type(arg->type, out, 0);
	out << ") msg->rcv_w" << nRcvReg++;
      }
      else
	out << "TheInputString.OP_" << op->name
	    << '.' << arg->name;
    }
    else /* sc_outformal */ {
      if (symbol_IsInterface(arg->type))
	out << "msg->snd_key" << nSndCap++;
      else if (can_registerize(arg->type) && nSndReg < MAX_REGS)
	out << '&' << arg->name;
      else
	out << "TheOutputString.OP_" << op->name
	    << '.' << arg->name;
    }

    comma = ", ";
  }
}
#endif

#if 0
static void
output_server_dispatch(std::ostream& out, GCPtr<Symbol> s, int indent)
{
  if (s->baseType) {
    GCPtr<Symbol> base = s->baseType;
    while (base->cls == sc_symRef)
      base = base->value;

    output_server_dispatch(out, base, indent);
  }

  out << "\n";
  do_indent(out, indent);
  out << "/* dispatch for " << s->QualifiedName() << " */\n";

  for(unsigned i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> op = s->children[i];
    if (op->cls != sc_operation)
      continue;

    do_indent(out, indent);
    out << "case KT_" 
	<< op->QualifiedName()
	<< ':' << endl;
    do_indent(out, indent+2);
    out << '{' << endl;

    {
      unsigned nSndReg = 1;
      for (unsigned a = 0; a < op->children.size(); a++) {
	GCPtr<Symbol> arg = symvec_fetch(op->children,a);

	if (arg->cls == sc_outformal 
	    && can_registerize(arg->type)
	    && nSndReg < MAX_REGS) {

	  do_indent(out, indent+4);
	  output_c_type(arg->type, out, 0);
	  out << ' ' << arg->name << ';'
	      << endl;
	}
      }
    }

    do_indent(out, indent+4);
    out << "/* Do something vaguely purposeful */\n";
    do_indent(out, indent+4);
    out << "msg.snd_code = OP_" 
	<< op->name
	<< "(";

    output_server_dispatch_args(out, op);

    out << ");\n";

    do_indent(out, indent+4);
    out <<"break;\n";
    do_indent(out, indent+2);
    out << "}\n";
  }
}
#endif

static void
output_server_dispatch(std::ostream& out, GCPtr<Symbol> s, int indent)
{
  if (s->baseType) {
    GCPtr<Symbol> base = s->baseType;
    while (base->cls == sc_symRef)
      base = base->value;

    output_server_dispatch(out, base, indent);
  }

  out << "\n";
  do_indent(out, indent);
  out << "/* dispatch for " 
      << s->QualifiedName('_')
      << " */\n";

  for(size_t i = 0; i < s->children.size(); i++) {
    GCPtr<Symbol> op = s->children[i];
    if (op->cls != sc_operation)
      continue;

    do_indent(out, indent);
    out << "case KT_" 
	<< op->QualifiedName('_')
	<< ":\n";
    do_indent(out, indent+2);
    out << "{\n";

    do_indent(out, indent+4);
    out << "done = OP_" 
	<< op->name
	<< "(pMsg";

    if (c_op_needs_message_string(op, sc_formal))
      out << ", (const OP_IS_" 
	  << op->name
	  << " *) pMsg->rcv_data";

    if (c_op_needs_message_string(op, sc_outformal))
      out << ", (OP_OS_" 
	  << op->name
	  << " *) pMsg->snd_data";

    out << ");\n";

    if (c_op_needs_message_string(op, sc_outformal)) {
      do_indent(out, indent+4);
      out << "pMsg->snd_len = sizeof(OP_OS_" 
	  << op->name
	  << ");\n";
    }

    do_indent(out, indent+4);
    out << "break;\n";
    do_indent(out, indent+2);
    out << "}\n";
  }
}

#if 0
static void
output_interface_dispatch(std::ostream& out, GCPtr<Symbol> s, int indent)
{
  fprintf(out, 
	  "unsigned\n"
	  "DISPATCH_" "%s(Message *pMsg)\n",
	  s->QualifiedName('_'));
  out << "{\n";

  do_indent(out, indent+2);
  out << "unsigned done = 0;\n\n";

  do_indent(out, indent+2);
  out << "switch(pMsg->rcv_code) {\n";

  output_server_dispatch(out, s, indent+4);

  do_indent(out, indent+4);
  out << "default:\n";

  do_indent(out, indent+6);
  out << "{\n";

  do_indent(out, indent+8);
  out << "pMsg->snd_code = RC_eros_key_UnknownRequest;\n";

  do_indent(out, indent+8);
  out << "break;\n";

  do_indent(out, indent+6);
  out << "}\n";

  do_indent(out, indent+2);
  out << "}\n";

  out << "\n";

  do_indent(out, indent+2);
  out << "return done;\n";

  out << "}\n\n";
}
#endif

#if 0
static void
output_server_dispatchers(std::ostream& out, GCPtr<Symbol> s, int indent)
{
  fprintf(out, "%s\n", preamble);

  output_interface_dispatch(out, s, indent);

#if 0
  out << "int"
      << endl
      << "PROCESS_"
      << s->QualifiedName('_')
      << "(void)\n";
  out << '{' << endl;
  do_indent(out, indent+2);
  out << "/* Kilroy was here */\n";

  do_indent(out, indent+2);
  out << "unsigned done = 0;\n";

  do_indent(out, indent+2);
  out << "Message msg;\n";

  if (needInputString || needOutputString) {
    out << "\n";

    if (needInputString) {
      do_indent(out, indent+2);
      out << "IF_IS_" << s->name << " TheInputString;\n";
    }

    if (needOutputString) {
      do_indent(out, indent+2);
      out << "IF_OS_" << s->name << " TheOutputString;\n";
    }

    out << "\n";
  }

  do_indent(out, indent+2);
  out << "msg.snd_code = 0;\n";
  do_indent(out, indent+2);
  out << "msg.snd_w1 = 0;\n";
  do_indent(out, indent+2);
  out << "msg.snd_w2 = 0;\n";
  do_indent(out, indent+2);
  out << "msg.snd_w3 = 0;\n";

  do_indent(out, indent+2);
  out << "msg.snd_invKey = KR_VOID;\n";
  do_indent(out, indent+2);
  out << "msg.snd_key0 = KR_VOID;\n";
  do_indent(out, indent+2);
  out << "msg.snd_key1 = KR_VOID;\n";
  do_indent(out, indent+2);
  out << "msg.snd_key2 = KR_VOID;\n";
  do_indent(out, indent+2);
  out << "msg.snd_rsmkey = KR_VOID;\n";

  do_indent(out, indent+2);
  out << "msg.rcv_key0 = KR_RETURN;\n";
  do_indent(out, indent+2);
  out << "msg.rcv_key1 = KR_ARG(0);\n";
  do_indent(out, indent+2);
  out << "msg.rcv_key2 = KR_ARG(1);\n";
  do_indent(out, indent+2);
  out << "msg.rcv_rsmkey = KR_ARG(2);\n";
  
  if (needInputString) {
    do_indent(out, indent+2);
    out << "msg.rcv_data = &TheInputString;\n";
    do_indent(out, indent+2);
    out << "msg.rcv_limit = sizeof(TheInputString);\n";
  }
  if (needOutputString) {
    do_indent(out, indent+2);
    out << "msg.snd_data = &TheOutputString;\n";
    do_indent(out, indent+2);
    out << "msg.snd_len = 0;\n";
  }

  out << "\n";

  do_indent(out, indent+2);
  out << "while (!done) {\n";

  do_indent(out, indent+4);
  out << "RETURN(&msg);\n";
  out << "\n";

  do_indent(out, indent+4);
  out << "msg.snd_invKey = KR_RETURN;\n";
  do_indent(out, indent+4);
  out << "msg.snd_key0 = KR_VOID;\n";
  do_indent(out, indent+4);
  out << "msg.snd_key1 = KR_VOID;\n";
  do_indent(out, indent+4);
  out << "msg.snd_key2 = KR_VOID;\n";
  do_indent(out, indent+4);
  out << "msg.snd_rsmkey = KR_VOID;\n";

  do_indent(out, indent+4);
  out << "msg.snd_code = 0;\n";
  do_indent(out, indent+4);
  out << "msg.snd_w1 = 0;\n";
  do_indent(out, indent+4);
  out << "msg.snd_w2 = 0;\n";
  do_indent(out, indent+4);
  out << "msg.snd_w3 = 0;\n";

  do_indent(out, indent+4);
  out << "msg.snd_len = 0;\n";

  out << "\n";

  do_indent(out, indent+4);
  out << "done = DISPATCH_"
      << s->QualifiedName('_')
      << "(&msg);"
      << endl
      << endl;

  do_indent(out, indent+4);
  out << "if (msg.snd_code != RC_OK) msg.snd_len = 0;\n";
  if (needInputString) {
    out << "\n";
    do_indent(out, indent+4);
    out << "msg.rcv_data = &TheInputString;\n";
    do_indent(out, indent+4);
    out << "msg.rcv_limit = sizeof(TheInputString);\n";
  }

  do_indent(out, indent+2);
  out << "}\n";

  out << "}\n";
#endif
}
#endif

static void
symdump(GCPtr<Symbol> s, std::ostream& out, const std::string& preamble,
	int indent)
{
  do_indent(out, indent);

  switch(s->cls){
  case sc_absinterface:
    // enabling stubs for abstract interface.  This can be double checked later
  case sc_interface:
    {
#if 0
      extern bool opt_dispatchers;

      if (opt_dispatchers) {
	extern const char *target;
	InternedString fileName;
	std::ostream& out;

	{
	  const char *sqn = s->QualifiedName('_');
	  char *tmp = 
	    VMALLOC(char,
		    strlen(target) + strlen("/") + strlen(sqn) 
		    + strlen(".c") + 1);

	  strcpy(tmp, target);
	  strcat(tmp, "/");
	  strcat(tmp, sqn);
	  strcat(tmp, ".c");

	  fileName = intern(tmp);
	  free(tmp);
	}

	out = fopen(fileName, "w+");
	if (out == NULL)
	  diag_fatal(1, "Couldn't open stub source file \"%s\"\n",
		      fileName);

	output_server_dispatchers(out, s, indent);
      }
#endif

      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out, preamble, indent);

      break;
    }
  case sc_package:
  case sc_enum:
  case sc_bitset:
    {
      for(size_t i = 0; i < s->children.size(); i++)
	symdump(s->children[i], out, preamble, indent);

      break;
    }
  case sc_struct:
    {
#if 0
      extern const char *target;
      InternedString fileName = target;
      fileName << "/";
      fileName << s->QualifiedName('_');
      fileName << ".c";

      cout << "  Creating: " << fileName << endl;

      ofstream out(fileName);
      if (!out.is_open())
	diag_fatal(1, "Couldn't open stub source file \"%s\"\n",
		    fileName);

      output_client_struct(out, s, indent);
      out.close();
#endif

      break;
    }

  case sc_operation:
    {
      if ((s->flags & SF_NOSTUB) == 0) {
	Path fileName;

	std::string sqn = s->QualifiedName('_');
	fileName = target + Path(sqn + ".c");

	std::ofstream out(fileName.c_str(), 
			  std::ios_base::out|std::ios_base::trunc);

	if (!out.is_open()) {
	  fprintf(stderr, "Couldn't open stub source file \"%s\"\n",
		  fileName.c_str());
	  exit(1);
	}

	output_client_stub(out, preamble, s, indent);

	out.close();
      }

      break;
    }
  case sc_const:
#if 0
    {
      BigNum mi = compute_value(s->value);
      fprintf(out, "const " "%s %s = %d;\n", 
	       c_typename(s->type), 
	       s->QualifiedName('_'), 
	       mpz_get_ui(&mi));
      break;
    }
#endif
  case sc_exception:
  case sc_typedef:
    {
      /* Ignore -- handled in header generator */
      break;
    }
  default:
    {
      out << "UNKNOWN/BAD SYMBOL CLASS " 
	  << s->ClassName()
	  << " FOR: "
	  << s->name
	  << "\n";
      break;
    }
  }
}

void
output_c_stubs(GCPtr<Symbol> s)
{
  SymVec vec;

  if (s->isActiveUOC == false)
    return;

  target.smkdir();

  if (s->IsFixedSerializable() == false) {
    fprintf(stderr, "Type \"%s\" is not serializable in bounded space\n", 
	    s->QualifiedName('.').c_str());
    exit(1);
  }

  std::stringstream preamble;

#if 0 // XXX we may need this
  preamble << "#if !defined(__STDC_VERSION__) ||"
	   << " (__STDC_VERSION__ < 19901L)\n";
  preamble << "#error \"bitcc output requires C99 support\"\n";
  preamble << "#endif\n";
#endif

  preamble << "#include <stdbool.h>\n";
  preamble << "#include <stddef.h>\n";
  preamble << "#include <alloca.h>\n";
#if 0
  preamble << "#include <eros/target.h>\n";
  preamble << "#include <eros/Invoke.h>\n";
  // preamble << "#include <eros/capidl.h>\n";
#endif

  s->ComputeDependencies(vec);

  vec.qsort(Symbol::CompareByQualifiedName);

  for (size_t i = 0; i < vec.size(); i++) {
    preamble << "#include <idl/";
    preamble << vec[i]->QualifiedName( '/');
    preamble << ".h>\n";
  }

  preamble << "#include <idl/";
  preamble << s->QualifiedName('/');
  preamble << ".h>\n";

  /* Blank line between #include's and content: */
  preamble << "\n";

  symdump(s, std::cout, preamble.str(), 0);
}
