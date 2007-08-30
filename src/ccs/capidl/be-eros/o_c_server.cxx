/*
 * Copyright (C) 2003, The EROS Group, LLC.
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
#include <errno.h>
#include <dirent.h>

#include <iostream>
#include <sstream>
#include <fstream>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include <libsherpa/Path.hxx>
#include "SymTab.hxx"
#include "util.hxx"
#include "capidl.hxx"
#include "backend.hxx"

#define FIRST_REG 1		/* reserve 1 for opcode/result code */
/* Size of native register */
#define REGISTER_BITS      32

#define max(a,b) ((a > b) ? (a) : (b))

extern unsigned compute_direct_bytes(SymVec& symVec);
extern unsigned compute_indirect_bytes(SymVec& symVec);
extern SymVec extract_registerizable_arguments(GCPtr<Symbol> s, SymClass sc);
extern SymVec extract_string_arguments(GCPtr<Symbol> s, SymClass sc);

extern unsigned compute_direct_bytes(SymVec& symVec);

extern void output_c_type(GCPtr<Symbol> s, std::ostream& out, int indent);
extern unsigned can_registerize(GCPtr<Symbol> s, unsigned nReg);
extern unsigned emit_symbol_align(const std::string& lenVar, 
				  std::ostream& out, 
				  int indent,
				  unsigned elemAlign, unsigned align);

static void
emit_pass_from_reg(std::ostream& out, GCPtr<Symbol> arg, unsigned regCount)
{
  GCPtr<Symbol> argType = arg->type->ResolveRef();
  GCPtr<Symbol> argBaseType = argType->ResolveType();
  unsigned bits = argBaseType->v.bn.as_uint32();
  unsigned bitsInput = 0;

  /* If the quantity is larger than a single register, we need to
     decode the rest of it: */

  while (bitsInput < bits) {
    if (bitsInput)
      out << " | (";

    out << "((";
    output_c_type(argType, out, 0);
    out << ") msg.rcv_w" << regCount << ")";

    if (bitsInput)
      out << "<< " << bitsInput << ")";

    bitsInput += REGISTER_BITS;
    regCount++;
  }
}

static void
emit_return_via_reg(std::ostream& out, GCPtr<Symbol> arg, int indent, unsigned regCount)
{
  GCPtr<Symbol> argType = arg->type->ResolveRef();
  GCPtr<Symbol> argBaseType = argType->ResolveType();
  unsigned bits = argBaseType->v.bn.as_uint32();
  unsigned bitsOutput = 0;

  /* If the quantity is larger than a single register, we need to
     decode the rest of it: */

  while (bitsOutput < bits) {
    do_indent(out, indent);
    out << "msg->snd_w" << regCount << " = ";

    if (bitsOutput)
      out << " (";

    out << arg->name;

    if (bitsOutput)
      out << ">> " << bitsOutput << ")";

    out << ";\n";

    bitsOutput += REGISTER_BITS;
    regCount++;
  }
}

static void
emit_op_dispatcher(GCPtr<Symbol> s, std::ostream& out)
{
  unsigned snd_regcount = FIRST_REG;
  unsigned needRegs;

  SymVec rcvRegs = extract_registerizable_arguments(s, sc_formal);
  SymVec sndRegs = extract_registerizable_arguments(s, sc_outformal);
  SymVec rcvString = extract_string_arguments(s, sc_formal);
  SymVec sndString = extract_string_arguments(s, sc_outformal);

  unsigned sndOffset = 0;
  unsigned rcvOffset = 0;
  unsigned rcvDirect = compute_direct_bytes(rcvString);
  unsigned sndDirect = compute_direct_bytes(sndString);

  /* Computing indirect bytes on rcvString/sndString vector will
     generate the wrong number, but a non-zero number tells us that
     there ARE some indirect bytes to consider. */
  unsigned rcvIndirect = compute_indirect_bytes(rcvString);
  unsigned sndIndirect = compute_indirect_bytes(sndString);

  rcvDirect = round_up(rcvDirect, 8);
  sndDirect = round_up(sndDirect, 8);

  out << "\n";
  out << "static void\n";
  out << "DISPATCH_OP_"
      << s->QualifiedName('_')
      << "(Message *msg, IfInfo *info)\n";

  out << "{\n";

  if (rcvIndirect || sndIndirect) {
    if (rcvIndirect) {
      do_indent(out, 2);
      out << "unsigned rcvIndir = "
	  << rcvDirect
	  << ";\n";
    }
    if (sndIndirect) {
      do_indent(out, 2);
      out << "unsigned sndIndir = "
	  << sndDirect
	  << ";\n";
    }

    out << "\n";
  }

  do_indent(out, 2);
  out << "/* Emit OP "
      << s->QualifiedName('_')
      << " */\n";

  /* Pass 1: emit the declarations for the direct variables */
  if (rcvRegs.size() || rcvString.size()) {
    do_indent(out, 2);
    out << "/* Incoming arguments */\n";

    if (rcvRegs.size()) {
      unsigned rcv_regcount = FIRST_REG;

      for (size_t i = 0; i < rcvRegs.size(); i++) {
	GCPtr<Symbol> arg = rcvRegs[i];
	GCPtr<Symbol> argType = arg->type->ResolveRef();

	do_indent(out, 2);
	output_c_type(argType, out, 0);
	out << " " << arg->name << " = ";

	emit_pass_from_reg(out, arg, rcv_regcount);
	out << ";\n";
	rcv_regcount += needRegs;
      }
    }

    if (rcvString.size()) {
      for (size_t i = 0; i < rcvString.size(); i++) {
	GCPtr<Symbol> arg = rcvString[i];
	GCPtr<Symbol> argType = arg->type->ResolveRef();
	GCPtr<Symbol> argBaseType = argType->ResolveType();

	rcvOffset = round_up(rcvOffset, argBaseType->alignof());

	do_indent(out, 2);
	output_c_type(argType, out, 0);
	out << " *" << arg->name << " = (";
	output_c_type(argType, out, 0);
	out << " *) (msg->rcv_data + " << rcvOffset << ");\n";

	rcvOffset += argBaseType->directSize();
      }
    }

    out << "\n";
  }

  if (sndString.size() || sndRegs.size()) {
    do_indent(out, 2);
    out << "/* Outgoing arguments */\n";

    /* For the outbound registerizables, we need to declare variables so
       that we can pass pointers of the expected type. Casting the word
       fields as in "(char *) &msg.rcv_w0" could create problems on
       depending on byte sex */
    if (sndRegs.size()) {
      for (size_t i = 0; i < sndRegs.size(); i++) {
	GCPtr<Symbol> arg = sndRegs[i];
	GCPtr<Symbol> argType = arg->type->ResolveRef();
	GCPtr<Symbol> argBaseType = argType->ResolveType();

	do_indent(out, 2);
	output_c_type(argType, out, 0);
	out << " " << arg->name << ";\n";

	sndOffset += argBaseType->directSize();
      }
    }

    if (sndString.size()) {
      for (size_t i = 0; i < sndString.size(); i++) {
	GCPtr<Symbol> arg = sndString[i];
	GCPtr<Symbol> argType = arg->type->ResolveRef();
	GCPtr<Symbol> argBaseType = argType->ResolveType();

	sndOffset = round_up(sndOffset, argBaseType->alignof());

	do_indent(out, 2);
	output_c_type(argType, out, 0);
	out << " *" << arg->name << " = (";
	output_c_type(argType, out, 0);
	out << " *) (msg->snd_data + " << sndOffset << ");\n";

	sndOffset += argBaseType->directSize();
      }
    }

    out << "\n";
  }

  /* Pass 2: patch the indirect arg data pointers */
  for (size_t i = 0; i < rcvString.size(); i++) {
    GCPtr<Symbol> arg = rcvString[i];
    GCPtr<Symbol> argType = arg->type->ResolveRef();
    GCPtr<Symbol> argBaseType = argType->ResolveType();

    if (argBaseType->IsVarSequenceType()) {
      do_indent(out, 2);
      out << arg->name << "->data = (";

      output_c_type(argBaseType->type, out, 0);

      out << " *) (msg->rcv_data + rcvIndir)\n";

      do_indent(out, 2);
      out << "rcvIndir += ("
	  << arg->name
	  << "->len * sizeof(*"
	  << arg->name
	  << "->data));\n";
    }
  }

  /* Pass 3: make the actual call */
  do_indent(out, 2);
  out << "msg->snd_code = implement_"
      << s->QualifiedName('_')
      << "(";

  {
    unsigned rcv_regcount = FIRST_REG;

    for (size_t i = 0; i < s->children.size(); i++) {
      GCPtr<Symbol> arg = s->children[i];
      GCPtr<Symbol> argType = arg->type->ResolveRef();
      GCPtr<Symbol> argBaseType = argType->ResolveType();

      if (i > 0)
	out << ", ";
      
      if (arg->cls == sc_formal) {
	if ((needRegs = can_registerize(argBaseType, rcv_regcount))) {
	  out << arg->name;
	}
	else {
	  out << "*" << arg->name;
	}
      }
      else {
	if ((needRegs = can_registerize(argBaseType, snd_regcount))) {
	  out << "/* OUT */ &"
	      << arg->name;
	  snd_regcount += needRegs;
	}
	else {
	  out << "/* OUT */ "
	      << arg->name;
	}
      }
    }
  }
  out << ");\n";

  do_indent(out, 2);
  out << "msg->snd_len = 0; /* Until otherwise proven */\n";

  out << "\n";

  /* Pass 4: pack the outgoing return string */
  if (sndRegs.size() || sndString.size()) {
    unsigned snd_regcount = FIRST_REG;
    unsigned curAlign = 0xfu;

    do_indent(out, 2);
    out << "if (msg->snd_code == RC_OK) {\n";

    for (size_t i = 0; i < s->children.size(); i++) {
      GCPtr<Symbol> arg = s->children[i];
      GCPtr<Symbol> argType = arg->type->ResolveRef();
      GCPtr<Symbol> argBaseType = argType->ResolveType();

      if (arg->cls == sc_formal)
	continue;

      if ((needRegs = can_registerize(argBaseType, snd_regcount))) {
	emit_return_via_reg(out, arg, 4, snd_regcount);
	snd_regcount += needRegs;
      }

      else if (argBaseType->IsVarSequenceType()) {
	unsigned elemAlign = argBaseType->alignof();

	curAlign = emit_symbol_align("sndIndir", out, 4,
				     elemAlign, curAlign);
	do_indent(out, 4);
	out << "__builtin_memcpy(msg.snd_data + sndIndir, "
	    << arg->name
	    << "->data, "
	    << arg->name
	    << "->len * sizeof(*"
	    << arg->name
	    << "->data));\n";

	do_indent(out, 4);
	out << "sndIndir += ("
	    << arg->name
	    << "->len * sizeof(*"
	    << arg->name
	    << "->data));\n";
      }
    }

    if (sndString.size()) {
      do_indent(out, 4);

      if (sndIndirect)
	out << "msg->snd_len = sndIndir;\n";
      else
	out << "msg->snd_len = "
	    << sndDirect
	    << ";\n";
    }

    do_indent(out, 2);
    out << "}\n";
  }

  out << "}\n";
}

static void
emit_if_decoder(GCPtr<Symbol> s, std::ostream& out)
{
  if (!s->isActiveUOC)
    return;

  out << "\n";
  out << "static void\n";
  out << "DISPATCH_IF_"
      << s->QualifiedName('_')
      << "(Message *msg, IfInfo *info)\n";
  out << "{\n";

  do_indent(out, 2);
  out << "switch(msg->rcv_code) {\n";

  while (s) {
    out << "\n";
    do_indent(out, 2);
    out << "/* IF "
	<< s->QualifiedName('_')
	<< " */\n";
    out << "\n";

    for(size_t i = 0; i < s->children.size(); i++) {
      GCPtr<Symbol> child = s->children[i];
      if (child->cls != sc_operation)
	continue;

      do_indent(out, 2);
      out << "case OC_"
	  << child->QualifiedName('_')
	  << ":\n";
      do_indent(out, 4);
      out << "DISPATCH_OP_"
	  << s->QualifiedName('_')
	  << "(msg, info);\n";
    }

    s = s->baseType ? s->baseType->ResolveRef() : NULL;
  }

  out << "\n";
  do_indent(out, 2);
  out << "default:\n";
  do_indent(out, 4);
  out << "msg->snd_code = RC_eros_key_UnknownRequest;\n";
  do_indent(out, 4);
  out << "return;\n";

  do_indent(out, 2);
  out << "}\n";

  out << "}\n";
}

static void
emit_decoders(GCPtr<Symbol> s, std::ostream& out)
{
  if (s->mark)
    return;

  s->mark = true;

  switch(s->cls) {
  case sc_absinterface:
  case sc_interface:
    {
      if (s->baseType)
	emit_decoders(s->baseType->ResolveRef(), out);

      for(size_t i = 0; i < s->children.size(); i++)
	emit_decoders(s->children[i], out);

      emit_if_decoder(s, out);

      return;
    }

  case sc_operation:
    {
      emit_op_dispatcher(s, out);

      return;
    }

  default:
    return;
  }

  return;
}

static size_t
msg_size(GCPtr<Symbol> s, SymClass sc)
{
  size_t sz = 0;

  switch(s->cls) {
  case sc_absinterface:
  case sc_interface:
    {
      for(size_t i = 0; i < s->children.size(); i++) {
	size_t childsz = msg_size(s->children[i], sc);
	sz = max(sz, childsz);
      }

#if 0
      out <<  "IF "
	  << (sc == sc_formal) ? "Receive" : "Send"
	  << " size for "
	  << s->QualifiedName('_')
	  << " is "
	  << sz
	  << "\n";
#endif

      return sz;
    }

  case sc_operation:
    {
      size_t rcvsz;
      size_t sndsz;

      SymVec rcvString = extract_string_arguments(s, sc_formal);
      SymVec sndString = extract_string_arguments(s, sc_outformal);

      rcvsz = compute_direct_bytes(rcvString);
      sndsz = compute_direct_bytes(sndString);

      rcvsz = round_up(rcvsz, 8);
      sndsz = round_up(sndsz, 8);

      rcvsz += compute_indirect_bytes(rcvString);
      sndsz += compute_indirect_bytes(sndString);

      sz = (sc == sc_outformal) ? sndsz : rcvsz;

#if 0
      out <<  "OP "
	  << (sc == sc_formal) ? "Receive" : "Send"
	  << " size for "
	  << s->QualifiedName('_')
	  << " is "
	  << sz
	  << " (rcv "
	  << rcvsz
	  << ", snd "
	  << sndsz
	  << ")\n";
#endif

      return sz;
    }

  default:
    return sz;
  }

  return 0;
}

static size_t
size_server_buffer(GCPtr<Symbol> scope, SymClass sc)
{
  size_t bufSz = 0;

  /* Export subordinate packages first! */
  for (size_t i = 0; i < scope->children.size(); i++) {
    GCPtr<Symbol> child = scope->children[i];
    if (child->cls != sc_package && child->isActiveUOC) {
      size_t msgsz = msg_size(child, sc);
      bufSz = max(bufSz, msgsz);
    }

    if (child->cls == sc_package) {
      size_t msgsz = size_server_buffer(child, sc);
      bufSz = max(bufSz, msgsz);
    }
  }

  return bufSz;
}

static void
calc_sym_depend(GCPtr<Symbol> s, SymVec& vec)
{
  switch(s->cls) {
  case sc_absinterface:
  case sc_interface:
    {
      s->ComputeDependencies(vec);

      {
	GCPtr<Symbol> targetUoc = s->UnitOfCompilation();
	if (!vec.contains(targetUoc))
	  vec.append(targetUoc);
      }

      for(size_t i = 0; i < s->children.size(); i++)
	calc_sym_depend(s->children[i], vec);
    }

  case sc_operation:
    {
      s->ComputeDependencies(vec);
      return;
    }

  default:
    return;
  }

  return;
}

static void
compute_server_dependencies(GCPtr<Symbol> scope, SymVec& vec)
{
  /* Export subordinate packages first! */
  for (size_t i = 0; i < scope->children.size(); i++) {
    GCPtr<Symbol> child = scope->children[i];

    if (child->cls != sc_package && child->isActiveUOC)
      calc_sym_depend(child, vec);

    if (child->cls == sc_package)
      compute_server_dependencies(child, vec);
  }

  return;
}

void
emit_server_decoders(GCPtr<Symbol> scope, std::ostream& out)
{
  /* Export subordinate packages first! */
  for (size_t i = 0; i < scope->children.size(); i++) {
    GCPtr<Symbol> child = scope->children[i];

    if (child->cls != sc_package && child->isActiveUOC)
      emit_decoders(child, out);

    if (child->cls == sc_package)
      emit_server_decoders(child, out);
  }

  return;
}

void 
emit_server_dispatcher(GCPtr<Symbol> scope, std::ostream& out)
{
  size_t rcvSz = size_server_buffer(scope, sc_formal);
  size_t sndSz = size_server_buffer(scope, sc_outformal);

  do_indent(out, 2);
  out << "\nvoid\nmainloop(void *globalState)\n{\n";
  do_indent(out, 2);
  out << "Message msg;\n";
  do_indent(out, 2);
  out << "IfInfo info;\n";
  do_indent(out, 2);
  out << "extern bool if_demux(Message *pMsg, IfInfo *);\n";

  do_indent(out, 2);
  out << "size_t sndSz = " << sndSz << ";\n";
  do_indent(out, 2);
  out << "size_t rcvSz = " << rcvSz << ";\n";
  do_indent(out, 2);
  out << "void *sndBuf = alloca(sndSz);\n";
  do_indent(out, 2);
  out << "void *rcvBuf = alloca(rcvSz);\n";

  out << "\n";

  do_indent(out, 2);
  out << "info.globalState = globalState;\n";
  do_indent(out, 2);
  out << "info.done = false;\n";

  out << "\n";

  do_indent(out, 2);
  out << "__builtin_memset(&msg,0,sizeof(msg));\n";
  do_indent(out, 2);
  out << "msg.rcv_limit = rcvSz;\n";
  do_indent(out, 2);
  out << "msg.rcv_data = rcvBuf;\n";
  do_indent(out, 2);
  out << "msg.rcv_key0 = KR_APP(0);\n";
  do_indent(out, 2);
  out << "msg.rcv_key1 = KR_APP(1);\n";
  do_indent(out, 2);
  out << "msg.rcv_key2 = KR_APP(2);\n";
  do_indent(out, 2);
  out << "msg.rcv_rsmkey = KR_RETURN;\n";
  do_indent(out, 2);
  out << "msg.snd_data = sndBuf;\n";

  out << "\n";

  do_indent(out, 2);
  out << "/* Initial return to void in order to become available\n";
  do_indent(out, 2);
  out << "   Note that memset() above has set send length to zero. */\n";
  do_indent(out, 2);
  out << "msg.snd_invKey = KR_VOID;\n";

  out << "\n";

  do_indent(out, 2);
  out << "do {\n";
  do_indent(out, 4);
  out << "RETURN(&msg);\n";

  out << "\n";

  do_indent(out, 4);
  out << "msg.snd_invKey = KR_RETURN;\t/* Until proven otherwise */\n";

  out << "\n";

  do_indent(out, 4);
  out << "/* Get some help from the server implementer to choose decoder\n";
  do_indent(out, 4);
  out << "   based on input key info field. The if_demux() routine is\n";
  do_indent(out, 4);
  out << "   also responsible for fetching register values and keys\n";
  do_indent(out, 4);
  out << "   that may have been stashed in a wrapper somewhere. */\n";

  out << "\n";

  do_indent(out, 4);
  out << "msg.snd_len = 0;\n";
  do_indent(out, 4);
  out << "msg.snd_key0 = 0;\n";
  do_indent(out, 4);
  out << "msg.snd_key1 = 0;\n";
  do_indent(out, 4);
  out << "msg.snd_key2 = 0;\n";
  do_indent(out, 4);
  out << "msg.snd_w0 = 0;\n";
  do_indent(out, 4);
  out << "msg.snd_w1 = 0;\n";
  do_indent(out, 4);
  out << "msg.snd_w2 = 0;\n";
  do_indent(out, 4);
  out << "msg.snd_code = RC_eros_key_UnknownRequest;\t/* Until otherwise proven */\n";

  out << "\n";

  do_indent(out, 4);
  out << "if (demux_if(&msg, &info))\n";
  do_indent(out, 6);
  out << "info.if_proc(&msg, &info);\n";
  do_indent(out, 2);
  out << "} while (!info.done);\n";
  out << "}\n";
}

void 
do_output_c_server(std::ostream& out, GCPtr<Symbol> universalScope,
		   BackEndFn fn)
{
  SymVec vec;

  compute_server_dependencies(universalScope, vec);

  std::stringstream preamble;

  preamble << "#if !defined(__STDC_VERSION__) ||"
	   << " (__STDC_VERSION__ < 19901L)\n";
  preamble << "#error \"bitcc output requires C99 support\"\n";
  preamble << "#endif\n";

  preamble << "#include <stdbool.h>\n";
  preamble << "#include <stddef.h>\n";
  preamble << "#include <alloca.h>\n";
  preamble << "#include <eros/target.h>\n";
  preamble << "#include <eros/Invoke.h>\n";
  preamble << "#include <domain/Runtime.h>\n";

  for (size_t i = 0; i < vec.size(); i++) {
    preamble << "#include <idl/";
    preamble << vec[i]->QualifiedName( '/');
    preamble << ".h>\n";
  }

  preamble << "#include \"";
  preamble << cHeaderName;
  preamble << "\"\n";

  out << preamble.str();

  out << "\ntypedef struct IfInfo {\n";
  do_indent(out, 2);
  out << "void (*if_proc)(Message *pMsg, struct IfInfo *ifInfo);\n";
  do_indent(out, 2);
  out << "void *invState;\t/* passed to handler */\n";
  do_indent(out, 2);
  out << "void *globalState;\t/* passed to handler */\n";
  do_indent(out, 2);
  out << "bool done;\t/* set true by handler when exiting */\n";
  out << "} IfInfo;\n";

  universalScope->ClearAllMarks();

  emit_server_decoders(universalScope, out);

  emit_server_dispatcher(universalScope, out);
}

void 
output_c_server(GCPtr<Symbol> universalScope, BackEndFn fn)
{
  if (outputFileName.isEmpty()) {
    fprintf(stderr, "Need to provide output file name.\n");
    exit(1);
  }

  if (outputFileName.asString() == "-")
    do_output_c_server(std::cout, universalScope, fn);
  else {
    std::ofstream out(outputFileName.c_str(),
		      std::ios_base::out|std::ios_base::trunc);
    if (!out.is_open()) {
      fprintf(stderr, "Couldn't open output file \"%s\" -- %s\n",
	      outputFileName.c_str(), strerror(errno));
      exit(1);
    }

    do_output_c_server(out, universalScope, fn);

    out.close();
  }
}
