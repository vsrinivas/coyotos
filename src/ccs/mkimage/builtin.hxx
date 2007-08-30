#ifndef BUILTIN_HXX
#define BUILTIN_HXX

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

#include <libsherpa/GCPtr.hxx>
#include <libsherpa/CVector.hxx>
#include "Value.hxx"

extern bool
isArgType(GCPtr<AST> arg, Value::ValKind);

extern void
requireArgType(InterpState& is, const std::string& nm,
	       CVector<GCPtr<Value> >& args, size_t ndx, Value::ValKind vk);

extern GCPtr<CapValue>
needCapArgType(InterpState& is, const std::string& nm,
	       CVector<GCPtr<Value> >& args, size_t ndx, CapType ct1, CapType ct2);

inline GCPtr<CapValue>
needCapArgType(InterpState& is, const std::string& nm,
	       CVector<GCPtr<Value> >& args, size_t ndx, CapType ct)
{
  return needCapArgType(is, nm, args, ndx, ct, ct);
}

extern GCPtr<CapValue>
needAnyCapArg(InterpState& is, const std::string& nm,
	      CVector<GCPtr<Value> >& args, size_t ndx);

extern GCPtr<CapValue>
needBankArg(InterpState& is, const std::string& nm,
	    CVector<GCPtr<Value> >& args, size_t ndx);

extern GCPtr<CapValue>
needSpaceArg(InterpState& is, const std::string& nm,
	     CVector<GCPtr<Value> >& args, size_t ndx);

extern GCPtr<IntValue>
needIntArg(InterpState& is, const std::string& nm,
	   CVector<GCPtr<Value> >& args, size_t ndx);

extern GCPtr<StringValue>
needStringArg(InterpState& is, const std::string& nm,
	      CVector<GCPtr<Value> >& args, size_t ndx);

extern GCPtr<Environment<Value> > getBuiltinEnv(GCPtr<CoyImage>);

extern GCPtr<Value>
pf_print_one(InterpState& is, GCPtr<Value> arg);

extern GCPtr<Value>
pf_doprint(PrimFnValue& pfv,
	   InterpState& is, 
	   sherpa::CVector<GCPtr<Value> >& args);

extern GCPtr<Value>
pf_doprint_tree(PrimFnValue& pfv,
		InterpState& is, 
		sherpa::CVector<GCPtr<Value> >& args);

extern GCPtr<Value>
pf_doprint_space(PrimFnValue& pfv,
		 InterpState& is, 
		 sherpa::CVector<GCPtr<Value> >& args);

// This one is not really a legal built-in:
extern GCPtr<Value>
pf_doprint_star(InterpState& is, 
		sherpa::CVector<GCPtr<Value> >& args);

extern GCPtr<Value>
pf_arith(PrimFnValue& pfv,
	 InterpState& is, 
	 sherpa::CVector<GCPtr<Value> >& args);

// Capability downgrade operations:
extern GCPtr<Value>
pf_downgrade(PrimFnValue& pfv,
	     InterpState& is, 
	     sherpa::CVector<GCPtr<Value> >& args);

extern GCPtr<Value>
pf_insert_subspace(PrimFnValue& pfv,
		   InterpState& is, 
		   sherpa::CVector<GCPtr<Value> >& args);

extern GCPtr<Value>
pf_readfile(PrimFnValue& pfv,
	    InterpState& is, 
	    sherpa::CVector<GCPtr<Value> >& args);

extern GCPtr<Value>
pf_loadimage(PrimFnValue& pfv,
	     InterpState& is, 
	     sherpa::CVector<GCPtr<Value> >& args);

extern GCPtr<Value>
pf_guard(PrimFnValue& pfv,
	 InterpState& is, 
	 sherpa::CVector<GCPtr<Value> >& args);

extern GCPtr<Value>
pf_guarded_space(PrimFnValue& pfv,
		 InterpState& is, 
		 sherpa::CVector<GCPtr<Value> >& args);

extern GCPtr<Value>
pf_mk_misccap(PrimFnValue& pfv,
	      InterpState& is, 
	      sherpa::CVector<GCPtr<Value> >& args);

// Fabricate window capability
extern GCPtr<Value>
pf_mk_window(PrimFnValue& pfv,
	     InterpState& is, 
	     sherpa::CVector<GCPtr<Value> >& args);

// Fabricate process
extern GCPtr<Value>
pf_mk_process(PrimFnValue& pfv,
	      InterpState& is, 
	      sherpa::CVector<GCPtr<Value> >& args);

// Fabricate endpoint
extern GCPtr<Value>
pf_mk_endpoint(PrimFnValue& pfv,
	       InterpState& is, 
	       sherpa::CVector<GCPtr<Value> >& args);

// Fabricate gpt
extern GCPtr<Value>
pf_mk_gpt(PrimFnValue& pfv,
	       InterpState& is, 
	       sherpa::CVector<GCPtr<Value> >& args);

// Fabricate page
extern GCPtr<Value>
pf_mk_page(PrimFnValue& pfv,
	   InterpState& is, 
	   sherpa::CVector<GCPtr<Value> >& args);

// Fabricate cappage
extern GCPtr<Value>
pf_mk_cappage(PrimFnValue& pfv,
	      InterpState& is, 
	      sherpa::CVector<GCPtr<Value> >& args);

// Wrap an existing capability:
extern GCPtr<Value>
pf_wrap(PrimFnValue& pfv,
	InterpState& is, 
	sherpa::CVector<GCPtr<Value> >& args);

// Make a sender capability:
extern GCPtr<Value>
pf_sendTo(PrimFnValue& pfv,
	  InterpState& is, 
	  sherpa::CVector<GCPtr<Value> >& args);

// Make a sender capability:
extern GCPtr<Value>
pf_dup(PrimFnValue& pfv,
       InterpState& is, 
       sherpa::CVector<GCPtr<Value> >& args);

#endif /* BUILTIN_HXX */
