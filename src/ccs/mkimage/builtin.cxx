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
#include <string>
#include <algorithm>

#include <libsherpa/utf8.hxx>
#include <libsherpa/Path.hxx>
#include <libcoyimage/ExecImage.hxx>

#include "Environment.hxx"
#include "Value.hxx"
#include "AST.hxx"
#include "builtin.hxx"
#include "Options.hxx"

using namespace sherpa;
using namespace std;

static GCPtr<Environment<Value> > builtins = 0;

/** @bug This ought to be pulled from the constant defined in the
 * Range IDL file. */
static const oid_t physOidStart = 0xff00000000000000ull;

static inline coyaddr_t 
align_up(coyaddr_t x, coyaddr_t y)
{
  return (x + y - 1) & ~(y-1);
}

bool
isArgType(GCPtr<Value> v, Value::ValKind vk)
{
  return (v->kind == vk);
}

void
requireArgType(InterpState& is, const std::string& nm,
	    CVector<GCPtr<Value> >& args, size_t ndx, Value::ValKind vk)
{
  GCPtr<Value> v = args[ndx];

  if (!isArgType(v, vk)) {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate argument type for argument "
		 << ndx << " to " 
		 << nm << "()\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

GCPtr<IntValue>
needIntArg(InterpState& is, const std::string& nm,
	   CVector<GCPtr<Value> >& args, size_t ndx)
{
  requireArgType(is, nm, args, ndx, Value::vk_int);
  return args[ndx].upcast<IntValue>();
}

GCPtr<StringValue>
needStringArg(InterpState& is, const std::string& nm,
	      CVector<GCPtr<Value> >& args, size_t ndx)
{
  requireArgType(is, nm, args, ndx, Value::vk_string);
  return args[ndx].upcast<StringValue>();
}

GCPtr<CapValue>
needCapArgType(InterpState& is, const std::string& nm,
	       CVector<GCPtr<Value> >& args, size_t ndx,
	       CapType ct1, CapType ct2)
{
  requireArgType(is, nm, args, ndx, Value::vk_cap);
  GCPtr<CapValue> cv = args[ndx].upcast<CapValue>();
  if ((cv->cap.type == ct1) || (cv->cap.type == ct2))
    return cv;

  is.errStream << is.curAST->loc << " "
	       << "Capability argument of type "
	       << cap_typeName(ct1);
  if (ct1 != ct2)
    is.errStream << " (or " << cap_typeName(ct2) << ")"
		 << " is required for argument "
		 << ndx << " to " 
		 << nm << "()\n";
  THROW(excpt::BadValue, "Bad interpreter result");
}

GCPtr<CapValue>
needAnyCapArg(InterpState& is, const std::string& nm,
	      CVector<GCPtr<Value> >& args, size_t ndx)
{
  requireArgType(is, nm, args, ndx, Value::vk_cap);
  return args[ndx].upcast<CapValue>();
}

GCPtr<CapValue>
needBankArg(InterpState& is, const std::string& nm,
	    CVector<GCPtr<Value> >& args, size_t ndx)
{
  needCapArgType(is, nm, args, ndx, ct_Entry);
  GCPtr<CapValue> cv = args[ndx].upcast<CapValue>();

  // Now this is truly a nuisance, but it is a useful error check to
  // validate that this entry capability might plausibly be a bank
  // capability:
  GCPtr<CiEndpoint> ep = is.ci->GetEndpoint(cv->cap);

  oid_t bankOID = ep->v.endpointID - 1; /* unbias the endpoint ID */
  if (bankOID >= is.ci->vec.bank.size())
    goto bad;

  if (is.ci->vec.bank[bankOID].oid != cv->cap.u2.oid)
    goto bad;

  return cv;

 bad:
  is.errStream << is.curAST->loc << " "
	       << "Space bank capability is required for argument "
	       << ndx << " to "
	       << nm << "()\n";
  THROW(excpt::BadValue, "Bad interpreter result");
}

GCPtr<CapValue>
needSpaceArg(InterpState& is, const std::string& nm,
	    CVector<GCPtr<Value> >& args, size_t ndx)
{
  requireArgType(is, nm, args, ndx, Value::vk_cap);
  GCPtr<CapValue> cv = args[ndx].upcast<CapValue>();
  if (cap_isMemory(cv->cap) || cap_isType(cv->cap, ct_Null))
    return cv;

  is.errStream << is.curAST->loc << " "
	       << "Address space capability required for argument "
	       << ndx << " to " 
	       << nm << "()\n";
  THROW(excpt::BadValue, "Bad interpreter result");
}

GCPtr<Value> 
pf_print_one(InterpState& is,
	     GCPtr<Value> v0)
{
  switch(v0->kind) {
  case Value::vk_int:
    {
      GCPtr<IntValue> i = v0.upcast<IntValue>();
      std::string s = i->bn.asString(10);
      std::cout << s;

      break;
    }
  case Value::vk_float:
    {
      GCPtr<FloatValue> f = v0.upcast<FloatValue>();
      std::cout << f->d;

      break;
    }
  case Value::vk_unit:
    {
      std::cout << "UNIT";
      break;
    }
  case Value::vk_cap:
    {
      GCPtr<CapValue> cv = v0.upcast<CapValue>();
      std::cout << cv->cap;
      break;
    }
  case Value::vk_string:
    {
      GCPtr<StringValue> sv = v0.upcast<StringValue>();
      const char *s = sv->s.c_str();
      const char *send = s + sv->s.size();

      while (s != send) {
	const char *snext;
	char utf8[7];
	uint32_t codePoint = sv->DecodeStringCharacter(s, &snext);
	unsigned len = utf8_encode(codePoint, utf8);

	for (unsigned pos = 0; pos < len; pos++) {
	  unsigned char c = utf8[pos];
	  std::cout << c;
	}

	s = snext;
      }

      break;
    }
  case Value::vk_env:
    {
      GCPtr<EnvValue> ev = v0.upcast<EnvValue>();
      GCPtr<Environment<Value> > env = ev->env;

      for (size_t i = 0; i < env->bindings.size(); i++) {
	std::cout << ((i == 0) ? "[ " : "\n  ")
		  << env->bindings[i]->nm << " = ";

	pf_print_one(is, env->bindings[i]->val);

	if (env->bindings[i]->flags)
	  std::cout << " (";

	std::string comma;

	if (env->bindings[i]->flags & BF_CONSTANT) {
	  std::cout << comma << "const";
	  comma = ", ";
	}
	if (env->bindings[i]->flags & BF_EXPORTED) {
	  std::cout << comma << "public";
	  comma = ", ";
	}
	if (env->bindings[i]->flags)
	  std::cout << ")";
      }
      std::cout << " ]";
      break;
    }
  case Value::vk_function:
    {
      GCPtr<FnValue> fn = v0.upcast<FnValue>();
      cout << "<fn " << fn->nm << ", " << 
	fn->args->children->size() << " args>";
      break;
    }
  case Value::vk_primfn:
    {
      GCPtr<PrimFnValue> fn = v0.upcast<PrimFnValue>();
      cout << "<primfn " << fn->nm << ", " << fn->minArgs;
      if (fn->maxArgs == 0)
	cout << "...";
      else if (fn->maxArgs != fn->minArgs)
	cout << "-" << fn->maxArgs;
      cout << " args>";
      break;
    }
  case Value::vk_primMethod:
    {
      GCPtr<PrimMethodValue> fn = v0.upcast<PrimMethodValue>();
      cout << "<primMethod " << fn->nm << ", " << fn->minArgs;
      if (fn->maxArgs == 0)
	cout << "...";
      else if (fn->maxArgs != fn->minArgs)
	cout << "-" << fn->maxArgs;
      cout << " args>";
      break;
    }
  default:
    {
      is.errStream << is.curAST->loc << " "
		   << "Don't know how to print value type " 
		   << v0->kind << '\n';
      THROW(excpt::BadValue, "Bad interpreter result");
    }
  }

  return &TheUnitValue;
}

GCPtr<Value> 
pf_doprint(PrimFnValue& pfv,
	   InterpState& is,
	   CVector<GCPtr<Value> >& args)
{
  for (size_t i = 0; i < args.size(); i++) {
    GCPtr<Value> val = args[i];

    pf_print_one(is, val);
  }

  return &TheUnitValue;
}

GCPtr<Value>
pf_doprint_star(InterpState& is,
		CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> v0 = args[0];

  if (v0->kind != Value::vk_cap) {
    // Be forgiving:
    pf_print_one(is, v0);
    std::cout << '\n';
    return &TheUnitValue;
  }

  GCPtr<CapValue> cv = v0.upcast<CapValue>();

  switch(cv->cap.type) {
  case ct_Process:
    {
      GCPtr<CiProcess> ob = is.ci->GetProcess(cv->cap);
      
      std::cout << *ob;
      break;
    }
  case ct_Endpoint:
    {
      GCPtr<CiEndpoint> ob = is.ci->GetEndpoint(cv->cap);
      
      std::cout << *ob;
      break;
    }
  case ct_GPT:
    {
      GCPtr<CiGPT> ob = is.ci->GetGPT(cv->cap);
      
      std::cout << *ob;
      break;
    }
  case ct_Page:
    {
      GCPtr<CiPage> ob = is.ci->GetPage(cv->cap);

      std::cout << *ob;
      break;
    }
  case ct_CapPage:
    {
      GCPtr<CiCapPage> ob = is.ci->GetCapPage(cv->cap);

      std::cout << *ob;
      break;
    }

  default:
    // Be forgiving:
    pf_print_one(is, v0);
    std::cout << '\n';
    break;
  }

  //  std::cout << '\n';
  return &TheUnitValue;
}

GCPtr<Value>
pf_fatal(PrimFnValue& pfv,
        InterpState& is,
        CVector<GCPtr<Value> >& args)
{
  GCPtr<StringValue> str = needStringArg(is, pfv.nm, args, 0);

  is.errStream << is.curAST->loc << " fatal: " << str->s << '\n';
  THROW(excpt::BadValue, "Bad interpreter result");
}

GCPtr<Value>
pf_add(PrimFnValue& pfv,
       InterpState& is,
       CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> vl = args[0];
  GCPtr<Value> vr = args[1];

  if (vl->kind == Value::vk_int && vr->kind == Value::vk_int) {
    GCPtr<IntValue> a = vl.upcast<IntValue>();
    GCPtr<IntValue> b = vr.upcast<IntValue>();

    return new IntValue(a->bn + b->bn);
  }
  else if (vl->kind == Value::vk_float && vr->kind == Value::vk_float) {
    GCPtr<FloatValue> a = vl.upcast<FloatValue>();
    GCPtr<FloatValue> b = vr.upcast<FloatValue>();

    return new FloatValue(a->d + b->d);
  }
  else {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate dynamic types to "
		 << pfv.nm << '\n';
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

GCPtr<Value>
pf_sub(PrimFnValue& pfv,
       InterpState& is,
       CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> vl = args[0];
  GCPtr<Value> vr = args[1];

  if (vl->kind == Value::vk_int && vr->kind == Value::vk_int) {
    GCPtr<IntValue> a = vl.upcast<IntValue>();
    GCPtr<IntValue> b = vr.upcast<IntValue>();

    return new IntValue(a->bn - b->bn);
  }
  else if (vl->kind == Value::vk_float && vr->kind == Value::vk_float) {
    GCPtr<FloatValue> a = vl.upcast<FloatValue>();
    GCPtr<FloatValue> b = vr.upcast<FloatValue>();

    return new FloatValue(a->d - b->d);
  }
  else {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate dynamic types to "
		 << pfv.nm << '\n';
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

GCPtr<Value>
pf_mul(PrimFnValue& pfv,
       InterpState& is,
       CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> vl = args[0];
  GCPtr<Value> vr = args[1];

  if (vl->kind == Value::vk_int && vr->kind == Value::vk_int) {
    GCPtr<IntValue> a = vl.upcast<IntValue>();
    GCPtr<IntValue> b = vr.upcast<IntValue>();

    return new IntValue(a->bn * b->bn);
  }
  else if (vl->kind == Value::vk_float && vr->kind == Value::vk_float) {
    GCPtr<FloatValue> a = vl.upcast<FloatValue>();
    GCPtr<FloatValue> b = vr.upcast<FloatValue>();

    return new FloatValue(a->d * b->d);
  }
  else {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate dynamic types to "
		 << pfv.nm << '\n';
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

GCPtr<Value>
pf_div(PrimFnValue& pfv,
       InterpState& is,
       CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> vl = args[0];
  GCPtr<Value> vr = args[1];

  if (vl->kind == Value::vk_int && vr->kind == Value::vk_int) {
    GCPtr<IntValue> a = vl.upcast<IntValue>();
    GCPtr<IntValue> b = vr.upcast<IntValue>();

    return new IntValue(a->bn / b->bn);
  }
  else if (vl->kind == Value::vk_float && vr->kind == Value::vk_float) {
    GCPtr<FloatValue> a = vl.upcast<FloatValue>();
    GCPtr<FloatValue> b = vr.upcast<FloatValue>();

    return new FloatValue(a->d / b->d);
  }
  else {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate dynamic types to "
		 << pfv.nm << '\n';
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

GCPtr<Value>
pf_mod(PrimFnValue& pfv,
       InterpState& is,
       CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> vl = args[0];
  GCPtr<Value> vr = args[1];

  if (vl->kind == Value::vk_int && vr->kind == Value::vk_int) {
    GCPtr<IntValue> a = vl.upcast<IntValue>();
    GCPtr<IntValue> b = vr.upcast<IntValue>();

    return new IntValue(a->bn % b->bn);
  }
  else {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate dynamic types to "
		 << pfv.nm << '\n';
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

GCPtr<Value>
pf_negate(PrimFnValue& pfv,
	  InterpState& is,
	  CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> vl = args[0];

  if (vl->kind == Value::vk_int) {
    GCPtr<IntValue> a = vl.upcast<IntValue>();

    return new IntValue(- a->bn);
  }
  else if (vl->kind == Value::vk_float) {
    GCPtr<FloatValue> a = vl.upcast<FloatValue>();

    return new FloatValue(- a->d);
  }
  else {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate dynamic types to "
		 << pfv.nm << '\n';
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

GCPtr<Value>
pf_cmp(PrimFnValue& pfv,
       InterpState& is,
       CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> vl = args[0];
  GCPtr<Value> vr = args[1];

  if (vl->kind == Value::vk_int && vr->kind == Value::vk_int) {
    GCPtr<IntValue> a = vl.upcast<IntValue>();
    GCPtr<IntValue> b = vr.upcast<IntValue>();

    switch(pfv.nm[0]) {
    case '<':
      return new IntValue (a->bn < b->bn);
    case '>':
      return new IntValue (a->bn > b->bn);
    default:
      THROW(excpt::BadValue, "Bad interpreter result");
    }
  }
  else if (vl->kind == Value::vk_float && vr->kind == Value::vk_float) {
    GCPtr<FloatValue> a = vl.upcast<FloatValue>();
    GCPtr<FloatValue> b = vr.upcast<FloatValue>();

    switch(pfv.nm[0]) {
    case '<':
      return new IntValue (a->d < b->d);
    case '>':
      return new IntValue (a->d > b->d);
    default:
      THROW(excpt::BadValue, "Bad interpreter result");
    }
  }
  else if (vl->kind == Value::vk_string && vr->kind == Value::vk_string) {
    GCPtr<StringValue> a = vl.upcast<StringValue>();
    GCPtr<StringValue> b = vr.upcast<StringValue>();

    switch(pfv.nm[0]) {
    case '<':
      return new IntValue (a->s.compare(b->s) < 0);
    case '>':
      return new IntValue (a->s.compare(b->s) > 0);
    default:
      THROW(excpt::BadValue, "Bad interpreter result");
    }
  }
  else {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate dynamic types to "
		 << pfv.nm << '\n';
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

GCPtr<Value>
pf_cmpe(PrimFnValue& pfv,
	InterpState& is,
	CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> vl = args[0];
  GCPtr<Value> vr = args[1];

  if (vl->kind == Value::vk_int && vr->kind == Value::vk_int) {
    GCPtr<IntValue> a = vl.upcast<IntValue>();
    GCPtr<IntValue> b = vr.upcast<IntValue>();

    switch(pfv.nm[0]) {
    case '<':
      return new IntValue (a->bn <= b->bn);
    case '>':
      return new IntValue (a->bn >= b->bn);
    case '=':
      return new IntValue (a->bn == b->bn);
    case '!':
      return new IntValue (a->bn != b->bn);
    default:
      THROW(excpt::BadValue, "Bad interpreter result");
    }
  }
  else if (vl->kind == Value::vk_float && vr->kind == Value::vk_float) {
    GCPtr<FloatValue> a = vl.upcast<FloatValue>();
    GCPtr<FloatValue> b = vr.upcast<FloatValue>();

    switch(pfv.nm[0]) {
    case '<':
      return new IntValue (a->d <= b->d);
    case '>':
      return new IntValue (a->d >= b->d);
    case '=':
      return new IntValue (a->d == b->d);
    case '!':
      return new IntValue (a->d != b->d);
    default:
      THROW(excpt::BadValue, "Bad interpreter result");
    }
  }
  else if (vl->kind == Value::vk_string && vr->kind == Value::vk_string) {
    GCPtr<StringValue> a = vl.upcast<StringValue>();
    GCPtr<StringValue> b = vr.upcast<StringValue>();

    switch(pfv.nm[0]) {
    case '<':
      return new IntValue (a->s.compare(b->s) <= 0);
    case '>':
      return new IntValue (a->s.compare(b->s) >= 0);
    case '=':
      return new IntValue (a->s.compare(b->s) == 0);
    case '!':
      return new IntValue (a->s.compare(b->s) != 0);
    default:
      THROW(excpt::BadValue, "Bad interpreter result");
    }
  } 
  else if (vl->kind == Value::vk_cap && vr->kind == Value::vk_cap &&
	(pfv.nm[0] == '=' || pfv.nm[0] == '!')) {
    /* capabilities can only be compared for equality */
    GCPtr<CapValue> a = vl.upcast<CapValue>();
    GCPtr<CapValue> b = vr.upcast<CapValue>();

    bool equal = (memcmp(&a->cap, &b->cap, sizeof (a->cap)) == 0);
    switch(pfv.nm[0]) {
    case '=':
      return new IntValue (equal);
    case '!':
      return new IntValue (!equal);
    default:
      THROW(excpt::BadValue, "Bad interpreter result");
    }
  }
  else {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate dynamic types to "
		 << pfv.nm << '\n';
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

GCPtr<Value>
pf_not(PrimFnValue& pfv,
       InterpState& is,
       CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> vl = args[0];

  if (vl->kind == Value::vk_int) {
    GCPtr<IntValue> a = vl.upcast<IntValue>();

    return new IntValue((a->bn == 0) ? 1 : 0);
  }
  else if (vl->kind == Value::vk_float) {
    GCPtr<FloatValue> a = vl.upcast<FloatValue>();

    return new IntValue((a->d == 0.0) ? 1 : 0);
  }
  else if (vl->kind == Value::vk_string) {
    GCPtr<StringValue> a = vl.upcast<StringValue>();

    return new IntValue(BigNum(0));
  }
  else {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate dynamic types to "
		 << pfv.nm << '\n';
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

GCPtr<Value>
pf_downgrade(PrimFnValue& pfv,
	     InterpState& is,
	     CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> v_cap = needAnyCapArg(is, pfv.nm, args, 0);
  GCPtr<CapValue> out = new CapValue(*v_cap);

  if (cap_isType(out->cap, ct_Null))
    return out;				/* Null caps are as weak as they get */

  if (!cap_isMemory(out->cap)) {
    is.errStream << is.curAST->loc << " "
		 << pfv.nm << "() requires a memory capability" << '\n';
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  if (pfv.nm == "opaque" || pfv.nm == "nocall") {
    if (out->cap.type != ct_GPT) {
      is.errStream << is.curAST->loc << " "
		   << "Argument to "
		   << pfv.nm
		   << "() must be a GPT capability.\n";
      THROW(excpt::BadValue, "Bad interpreter result");
    }
  }

  if (pfv.nm == "nocache" || pfv.nm == "writethrough") {
    if ((out->cap.type != ct_Page) ||
	(out->cap.u2.oid < physOidStart)) {
      is.errStream << is.curAST->loc << " "
		   << "Argument to "
		   << pfv.nm
		   << "() must be a Physical page frame capability.\n";
      THROW(excpt::BadValue, "Bad interpreter result");
    }
  }

  if (pfv.nm == "readonly") {
    out->cap.restr |= CAP_RESTR_RO;
    return out;
  }

  if (pfv.nm == "weaken") {
    out->cap.restr |= CAP_RESTR_WK | CAP_RESTR_RO;
    return out;
  }

  if (pfv.nm == "noexec") {
    out->cap.restr |= CAP_RESTR_NX;
    return out;
  }

  if (pfv.nm == "opaque") {
    out->cap.restr |= CAP_RESTR_OP;
    return out;
  }

  if (pfv.nm == "nocall") {
    out->cap.restr |= CAP_RESTR_NC;
    return out;
  }

  if (pfv.nm == "nocache") {
    out->cap.restr |= CAP_RESTR_CD;
    return out;
  }

#if 0
  if (pfv.nm == "writethrough") {
    out->cap.restr |= CAP_RESTR_WT;
    return out;
  }
#endif

  is.errStream << is.curAST->loc << " "
	       << "Buggered mkimage binding of \""
	       << pfv.nm << "\" in builtin.cxx.\n";
  THROW(excpt::BadValue, "Bad interpreter result");
}

GCPtr<Value>
pf_guard_cap(PrimFnValue& pfv,
	     InterpState& is,
	     CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> v_cap = needAnyCapArg(is, pfv.nm, args, 0);
  GCPtr<IntValue> v_offset = needIntArg(is, pfv.nm, args, 1);

  if (!cap_isMemory(v_cap->cap)) {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate capability type to "
		 << pfv.nm << "()\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  GCPtr<CapValue> ocap = new CapValue(*v_cap);

  // This function relies on the layout pun for window capabilities.
  coyaddr_t guard = v_offset->bn.as_uint64();
  uint32_t l2g = ocap->cap.u1.mem.l2g;

  if (args.size() > 2) {
    GCPtr<IntValue> v_l2g = needIntArg(is, pfv.nm, args, 2);
    l2g = v_l2g->bn.as_uint32();
  }

  if (l2g > (8*sizeof(coyaddr_t)) ||
      l2g < is.ci->target.pagel2v) {
    is.errStream << is.curAST->loc << " "
		 << "l2g argument to "
		 << pfv.nm << "() must be in the range "
		 << is.ci->target.pagel2v
		 << "-"
		 << (8*sizeof(coyaddr_t))
		 << "\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }
  if ((guard & (((1 << (l2g - 1)) << 1) - 1)) != 0) {
    is.errStream << is.curAST->loc << " "
		 << "guard value has bits below 2^(l2g) in call to "
		 << pfv.nm << "()\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }
  
  ocap->cap.u1.mem.l2g = l2g;
  ocap->cap.u1.mem.match = ((guard >> (l2g - 1)) >> 1);
  
  coyaddr_t effGuard = (ocap->cap.u1.mem.match << (l2g - 1)) << 1;
  if (guard != effGuard) {
    is.errStream << is.curAST->loc << " "
	      << "guard value truncated in call to "
	      << pfv.nm << "()\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }
  
  return ocap;
}

GCPtr<Value>
pf_memcap_ops(PrimFnValue& pfv,
	      InterpState& is,
	      CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> v_cap = needAnyCapArg(is, pfv.nm, args, 0);

  if (! (cap_isMemory(v_cap->cap) && cap_isObjectCap(v_cap->cap)) ) {
    is.errStream << is.curAST->loc << " "
		 << "Inappropriate capability type to "
		 << pfv.nm << "()\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  // This function relies on the layout pun for window capabilities.
  uint32_t match = v_cap->cap.u1.mem.match;
  uint32_t l2g =   v_cap->cap.u1.mem.l2g;

  if (pfv.nm == "get_l2g")
    return new IntValue(l2g);

  if (pfv.nm == "get_match")
    return new IntValue(match);

  is.errStream << is.curAST->loc << " "
	       << "Buggered mkimage binding of \""
	       << pfv.nm << "\" in builtin.cxx.\n";
  THROW(excpt::BadValue, "Bad interpreter result");
}

GCPtr<Value>
pf_mk_raw_obcap(PrimFnValue& pfv,
		InterpState& is,
		CVector<GCPtr<Value> >& args)
{
  GCPtr<IntValue> v_oid = needIntArg(is, pfv.nm, args, 0);
  oid_t oid = v_oid->bn.as_uint64();

  if (pfv.nm == "PageCap")
    return new CapValue(is.ci, is.ci->CiCap(ct_Page, oid));

  if (pfv.nm == "PhysPageCap")
    return new CapValue(is.ci, is.ci->CiCap(ct_Page, oid + physOidStart));

  if (pfv.nm == "CapCap")
    return new CapValue(is.ci, is.ci->CiCap(ct_CapPage, oid));

  if (pfv.nm == "GptCap")
    return new CapValue(is.ci, is.ci->CiCap(ct_GPT, oid));

  if (pfv.nm == "ProcessCap")
    return new CapValue(is.ci, is.ci->CiCap(ct_Process, oid));

  if (pfv.nm == "EndpointCap")
    return new CapValue(is.ci, is.ci->CiCap(ct_Endpoint, oid));

  is.errStream << is.curAST->loc << " "
	       << "Buggered mkimage binding of \""
	       << pfv.nm << "\" in builtin.cxx.\n";
  THROW(excpt::BadValue, "Bad interpreter result");
}

GCPtr<Value>
pf_guarded_space(PrimFnValue& pfv,
		 InterpState& is,
		 CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> bank = needBankArg(is, pfv.nm, args, 0);
  GCPtr<CapValue> spc = needSpaceArg(is, pfv.nm, args, 1);
  GCPtr<IntValue> offset = needIntArg(is, pfv.nm, args, 2);
  uint32_t l2g = 0;
  if (args.size() > 3)
    l2g = needIntArg(is, pfv.nm, args, 3)->bn.as_uint32();

  return new CapValue(is.ci, 
		      is.ci->MakeGuardedSubSpace(bank->cap,
						 spc->cap, 
						 offset->bn.as_uint64(),
						 l2g));
}

GCPtr<Value>
pf_insert_subspace(PrimFnValue& pfv,
		   InterpState& is,
		   sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> bank = needBankArg(is, pfv.nm, args, 0);
  GCPtr<CapValue> spc = needSpaceArg(is, pfv.nm, args, 1);
  GCPtr<CapValue> subspc = needSpaceArg(is, pfv.nm, args, 2);
  GCPtr<IntValue> offset = needIntArg(is, pfv.nm, args, 3);
  uint32_t l2arg = 0;

  if (args.size() > 4)
    l2arg = needIntArg(is, pfv.nm, args, 4)->bn.as_uint32();

  return new CapValue(is.ci, 
		      is.ci->AddSubSpaceToSpace(bank->cap, 
						spc->cap,
						offset->bn.as_uint64(),
						subspc->cap,
						l2arg));
}

GCPtr<Value>
pf_lookup_addr(PrimFnValue& pfv,
		   InterpState& is,
		   sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> spc = needSpaceArg(is, pfv.nm, args, 0);
  GCPtr<IntValue> offset = needIntArg(is, pfv.nm, args, 1);

  // bounds check?
  CoyImage::LeafInfo li = 
    is.ci->LeafAtAddr(spc->cap, offset->bn.as_uint64());

  GCPtr<Environment<Value> > env = new Environment<Value>();

  env->addConstant("cap", new CapValue(is.ci, li.cap));
  env->addConstant("offset", new IntValue(li.offset));
  env->addConstant("ro", new IntValue(!!(li.restr & CAP_RESTR_RO)));
  env->addConstant("nx", new IntValue(!!(li.restr & CAP_RESTR_NX)));
  env->addConstant("wk", new IntValue(!!(li.restr & CAP_RESTR_WK)));
  env->addConstant("op", new IntValue(!!(li.restr & CAP_RESTR_OP)));

  return new EnvValue(env);
}

GCPtr<Value>
pf_readfile(PrimFnValue& pfv,
	    InterpState& is,
	    sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> bank = needBankArg(is, pfv.nm, args, 0);
  GCPtr<StringValue> str = needStringArg(is, pfv.nm, args, 1);
  coyaddr_t offset = 0;
  if (args.size() > 2)
    offset = needIntArg(is, pfv.nm, args, 2)->bn.as_uint64();;

  std::string fileName = str->s;
  GCPtr<Path> file = Options::resolveLibraryPath(str->s);

  if (!file || !file->exists()) {
    is.errStream << is.curAST->loc << " "
		 << "file \"" << fileName << "\" not found.";
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  Path::portstat_t ps = file->stat();

  if (ps.type != Path::psFile) {
    is.errStream << is.curAST->loc << " "
		 << pfv.nm << "() filename argument must name a regular file.\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  if (offset & (is.ci->target.pageSize -1u)) {
    is.errStream << is.curAST->loc << " "
		 << pfv.nm << "() offset must be a pagesize-multiple\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  ifstream ifs(file->asString().c_str(), ifstream::in | ifstream::binary);
  if (ifs.fail()) {
    is.errStream << is.curAST->loc << " "
		 << pfv.nm << "() unable to open \"" << file->asString() 
		 << "\"\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  const size_t pagesz = is.ci->target.pageSize;

  capability space = is.ci->CiCap();

  for (off_t pos = 0; pos < ps.len; pos += pagesz) {
    capability pageCap = is.ci->AllocPage(bank->cap);
    GCPtr<CiPage> page = is.ci->GetPage(pageCap);
    
    ifs.read((char *)page->data, pagesz);
    space = is.ci->AddSubSpaceToSpace(bank->cap,
				      space,
				      pos + offset,
				      pageCap);
  }

  return new CapValue(is.ci, space);
}

GCPtr<Value>
pf_loadimage(PrimFnValue& pfv,
	     InterpState& is,
	     sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> bank = needBankArg(is, pfv.nm, args, 0);
  GCPtr<StringValue> str = needStringArg(is, pfv.nm, args, 1);

  GCPtr<Path> file = Options::resolveLibraryPath(str->s);

  if (!file || !file->exists()) {
    is.errStream << is.curAST->loc << " "
		 << "file \"" << str->s << "\" not found.";
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  Path::portstat_t ps = file->stat();

  if (ps.type != Path::psFile) {
    is.errStream << is.curAST->loc << " "
		 << pfv.nm << "() filename argument must name a regular file.\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  // Our address space construction strategy is very brute force, and
  // it will fail (with notice) if the page offsets within the file
  // and the page offsets within the address space pages do not
  // align. 
  //
  // What we are trying to accomplish here is page sharing for pages
  // that are (a) multiply mapped, or (b) overlap a boundary from one
  // region to another. Case (a) can occur if text/data have widely
  // separated virtual addresses, but some of the data appears in the
  // same file page as the end of text. Case (b) occurs when _etext ==
  // _edata and some of the data appears in the same file page as the
  // end of text.
  //
  // Because of the sharing issue, we proceed as follows:
  //
  //  For each page of the file
  //    For each base address at which the page is loadable (if any)
  //      Scan all loadable regions to see if they overlap that page
  //        virtual address.
  //      Take the union (positive logic) of the permissions of these
  //        regions as the page permissions. Rationale: if any region
  //        requires write, and the page VMA is identical, then the
  //        mapping must permit write.
  //      Map the page at this address with these permisions.
  //
  // There is probably a simpler way to do this, but binaries
  // typically have no more than two or three loadable regions. In
  // light of this, an N^2 algorithm seems tolerable here, and this
  // algorithm will be visibly related to the one used for symbol
  // table loads later.

  // We are going to iterate through each page of the file,
  // checking if it overlaps with one or more regions.
  ExecImage ei(file->asString());

  if (ei.elfMachine != is.ci->target.elfMachine) {
    is.errStream << is.curAST->loc << " "
		 << "file \"" << file->asString()
		 << "\" is not an executable for target "
		 << is.ci->target.name;
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  ei.sortByOffset();

  GCPtr<Environment<Value> > imageEnv = new Environment<Value>();
  imageEnv->addConstant("pc", new IntValue(ei.entryPoint));

  capability addrSpace = is.ci->CiCap();

  for (off_t offset = 0; offset < ps.len; offset += is.ci->target.pageSize) {
    off_t bound = min((off_t)(offset + is.ci->target.pageSize), ps.len);

    bool overlaps = false;
    // Make an initial pass to see if this page of the file is
    // needed. This is purely to avoid allocating excess pages in the
    // image:
    for (size_t r = 0; r < ei.region.size(); r++) {
      ExecRegion &rgn = ei.region[r];

      if (rgn.fileOffsetOverlaps(offset, bound)) {
	overlaps = true;
	break;
      }
    }

    if (!overlaps)
      continue;

    off_t minoffset = bound; // minimum page offset used.  Start at end-of-page
    off_t maxoffset = offset;// maximum offset used.  Starts at start-of-page

    // Do a second pass to determine the range of data used.  We need to
    // zero anything outside of that.
    for (size_t r = 0; r < ei.region.size(); r++) {
      ExecRegion &rgn = ei.region[r];

      if (!rgn.fileOffsetOverlaps(offset, bound))
	continue;

      if ((off_t)rgn.offset < minoffset)
	minoffset = rgn.offset;
      if ((off_t)(rgn.offset + rgn.filesz) > maxoffset)
	maxoffset = rgn.offset + rgn.filesz;
    }

    // Load the page:
    capability pgCap = is.ci->AllocPage(bank->cap);
    GCPtr<CiPage> pg = is.ci->GetPage(pgCap);
    memcpy(pg->data, ei.image.substr(offset, bound - offset).c_str(),
	   bound - offset);

    // If any part of the page is unused, zero it
    if (minoffset > offset)
      memset(pg->data, 0, minoffset - offset);
    if (maxoffset < bound)
      memset(pg->data + (maxoffset - offset), 0, (bound - maxoffset));

    // Insert the page with appropriate permissions at each region
    // where it appears:
    for (size_t r = 0; r < ei.region.size(); r++) {
      ExecRegion &rgn = ei.region[r];

      if (!rgn.fileOffsetOverlaps(offset, bound))
	continue;

      assert((rgn.offset % is.ci->target.pageSize) ==
	     (rgn.vaddr  % is.ci->target.pageSize));

      uint32_t perm = 0;

      // Page overlaps current region. Work out the page virtual
      // address of this mapping:
      uint64_t vaddr = rgn.vaddr;
      if ((off_t)rgn.offset > offset)
	vaddr -= (rgn.offset - offset);
      else
	vaddr += (offset - rgn.offset);

      uint64_t vbound = vaddr + is.ci->target.pageSize;

      assert ((vaddr % is.ci->target.pageSize) == 0);

      // Make a second pass over the regions to collect permissions
      // based on virtual page address overlap:
      for (size_t rv = 0; rv < ei.region.size(); rv++) {
	ExecRegion &rgn = ei.region[rv];
	if (rgn.vAddrOverlaps(vaddr, vbound))
	  perm |= rgn.perm;
      }

      // This page needs to be inserted at /vaddr/ with permissions
      // /perm/:
      pgCap.restr =
        (((perm & ER_X) == 0) ? CAP_RESTR_NX : 0) |
        (((perm & ER_W) == 0) ? CAP_RESTR_RO : 0);

#ifdef DEBUG
      std::ios_base::fmtflags flgs = is.errStream.flags();

      is.errStream << "Calling addSubspace at " 
		   << std::hex
		   << std::showbase
		   << er.vaddr + pos
		   << std::dec
		   << std::endl;

      is.errStream.flags(flgs);

      is.errStream << "  Space    = " << addrSpace << endl;
      is.errStream << "  SubSpace = " << pgCap << endl;
#endif

      addrSpace = 
	is.ci->AddSubSpaceToSpace(bank->cap, addrSpace, vaddr, pgCap);
#ifdef DEBUG
      is.errStream << "  Result   = " << addrSpace << endl;
#endif
    }
  }

  // Now do a post-pass to deal with missing BSS pages. These
  // are not backed by anything in the file, and the pass above will
  // not have added them because it checks for overlap based on the
  // filesize field.
  ei.sortByAddress();
  coyaddr_t vaddr = 0;
  for (size_t r = 0; r < ei.region.size(); r++) {
    ExecRegion &rgn = ei.region[r];

    // Skip any part of this region that is backed by the file. Round
    // up because we chunked the file into pages when we were loading it:
    coyaddr_t rvaddr = rgn.vaddr + rgn.filesz;
    coyaddr_t rvbound = rgn.vaddr + rgn.memsz;
    rvaddr = align_up(rvaddr, is.ci->target.pageSize);
    rvbound = align_up(rvbound, is.ci->target.pageSize);

    vaddr = max(vaddr, rvaddr);

    while (vaddr < rvbound) {
      capability pgCap = is.ci->AllocPage(bank->cap);
      GCPtr<CiPage> pg = is.ci->GetPage(pgCap);
      memset(pg->data, 0, is.ci->target.pageSize);
      
      coyaddr_t pgvbound = vaddr + is.ci->target.pageSize;

      uint32_t perm = 0;

      // Make a pass over the regions to collect permissions
      // based on virtual page address overlap:
      for (size_t rv = 0; rv < ei.region.size(); rv++) {
	ExecRegion &rgn = ei.region[rv];
	if (rgn.vAddrOverlaps(vaddr, pgvbound))
	  perm |= rgn.perm;
      }

      // This page needs to be inserted at /vaddr/ with permissions
      // /perm/:
      pgCap.restr =
        (((perm & ER_X) == 0) ? CAP_RESTR_NX : 0) |
        (((perm & ER_W) == 0) ? CAP_RESTR_RO : 0);

      addrSpace = 
	is.ci->AddSubSpaceToSpace(bank->cap, addrSpace, vaddr, pgCap);

      vaddr += is.ci->target.pageSize;
    }
  }

  imageEnv->addBinding("space", new CapValue(is.ci, addrSpace));

  return new EnvValue(imageEnv);
}


GCPtr<Value>
pf_mk_misccap(PrimFnValue& pfv,
		InterpState& is,
		sherpa::CVector<GCPtr<Value> >& args)
{
  if (pfv.nm == "NullCap")
    return new CapValue(is.ci, is.ci->CiCap());

  // Window, Background handled in pf_mk_window()
  if (pfv.nm == "KeyBits")
    return new CapValue(is.ci, is.ci->CiCap(ct_CapBits));

  if (pfv.nm == "Discrim")
    return new CapValue(is.ci, is.ci->CiCap(ct_Discrim));

  if (pfv.nm == "Range")
    return new CapValue(is.ci, is.ci->CiCap(ct_Range));

  if (pfv.nm == "Sleep")
    return new CapValue(is.ci, is.ci->CiCap(ct_Sleep));

  if (pfv.nm == "IrqCtl")
    return new CapValue(is.ci, is.ci->CiCap(ct_IrqCtl));

  if (pfv.nm == "SchedCtl")
    return new CapValue(is.ci, is.ci->CiCap(ct_SchedCtl));

  if (pfv.nm == "Checkpoint")
    return new CapValue(is.ci, is.ci->CiCap(ct_Checkpoint));

  if (pfv.nm == "ObStore")
    return new CapValue(is.ci, is.ci->CiCap(ct_ObStore));

  if (pfv.nm == "IoPerm")
    return new CapValue(is.ci, is.ci->CiCap(ct_IOPriv));

  if (pfv.nm == "PinCtl")
    return new CapValue(is.ci, is.ci->CiCap(ct_PinCtl));

  if (pfv.nm == "SysCtl")
    return new CapValue(is.ci, is.ci->CiCap(ct_SysCtl));

  if (pfv.nm == "KernLog")
    return new CapValue(is.ci, is.ci->CiCap(ct_KernLog));

  is.errStream << is.curAST->loc << " "
	       << "Buggered mkimage binding of \""
	       << pfv.nm << "\" in builtin.cxx.\n";
  THROW(excpt::BadValue, "Bad interpreter result");
}

GCPtr<Value>
pf_mk_window(PrimFnValue& pfv,
	     InterpState& is,
	     sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<IntValue> offset = needIntArg(is, pfv.nm, args, 1);

  if (pfv.nm == "Window") {
    GCPtr<CapValue> cv = new CapValue(is.ci, is.ci->CiCap(ct_Window));
    cv->cap.u2.offset = offset->bn.as_uint64();
    return cv;
  }

  if (pfv.nm == "LocalWindow") {
    GCPtr<IntValue> slot = needIntArg(is, pfv.nm, args, 2);
    GCPtr<CapValue> cv = new CapValue(is.ci, is.ci->CiCap(ct_LocalWindow));
    cv->cap.u2.offset = offset->bn.as_uint64();
    cv->cap.allocCount = slot->bn.as_uint32();
    return cv;
  }

  is.errStream << is.curAST->loc << " "
	       << "Buggered mkimage binding of \""
	       << pfv.nm << "\" in builtin.cxx.\n";
  THROW(excpt::BadValue, "Bad interpreter result");
}

GCPtr<Value>
pf_mk_process(PrimFnValue& pfv,
	      InterpState& is,
	      sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> bank = needBankArg(is, pfv.nm, args, 0);
  return new CapValue(is.ci, is.ci->AllocProcess(bank->cap));
}

GCPtr<Value>
pf_mk_bank(PrimFnValue& pfv,
	      InterpState& is,
	      sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> bank = needBankArg(is, pfv.nm, args, 0);
  return new CapValue(is.ci, is.ci->AllocBank(bank->cap));
}

GCPtr<Value>
pf_mk_gpt(PrimFnValue& pfv,
	  InterpState& is,
	  sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> bank = needBankArg(is, pfv.nm, args, 0);
  return new CapValue(is.ci, is.ci->AllocGPT(bank->cap));
}

GCPtr<Value>
pf_mk_endpoint(PrimFnValue& pfv,
	       InterpState& is,
	       sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> bank = needBankArg(is, pfv.nm, args, 0);
  return new CapValue(is.ci, is.ci->AllocEndpoint(bank->cap));
}

GCPtr<Value>
pf_mk_page(PrimFnValue& pfv,
	   InterpState& is,
	   sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> bank = needBankArg(is, pfv.nm, args, 0);
  return new CapValue(is.ci, is.ci->AllocPage(bank->cap));
}

GCPtr<Value>
pf_mk_cappage(PrimFnValue& pfv,
	      InterpState& is,
	      sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> bank = needBankArg(is, pfv.nm, args, 0);
  return new CapValue(is.ci, is.ci->AllocCapPage(bank->cap));
}

GCPtr<Value>
pf_mk_entry(PrimFnValue& pfv,
	    InterpState& is,
	    CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> cap = needCapArgType(is, pfv.nm, args, 0, ct_Endpoint, ct_Endpoint);
  GCPtr<IntValue> pp = needIntArg(is, pfv.nm, args, 1);

  GCPtr<CapValue> cv = new CapValue(*cap);
  GCPtr<CiEndpoint> ep = is.ci->GetEndpoint(cv->cap);

  cv->cap.type = ct_Entry;
  cv->cap.u1.protPayload = pp->bn.as_uint32();

  return cv;
}

GCPtr<Value>
pf_mk_appint(PrimFnValue& pfv,
	    InterpState& is,
	    CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> cap = needCapArgType(is, pfv.nm, args, 0, ct_Endpoint, ct_Endpoint);
  GCPtr<IntValue> maskBN = needIntArg(is, pfv.nm, args, 1);

  uint32_t mask = maskBN->bn.as_uint32();

  {
    GCPtr<CapValue> cv = new CapValue(*cap);
    GCPtr<CiEndpoint> ep = is.ci->GetEndpoint(cv->cap);

    cv->cap.type = ct_AppNotice;
    cv->cap.u1.protPayload = mask;

    return cv;
  }
}

GCPtr<Value>
pf_dup(PrimFnValue& pfv,
	InterpState& is,
	CVector<GCPtr<Value> >& args)
{
  GCPtr<CapValue> bank = needBankArg(is, pfv.nm, args, 0);
  GCPtr<CapValue> cap = needAnyCapArg(is, pfv.nm, args, 1);

  switch(cap->cap.type) {
  case ct_Endpoint:
    {
      GCPtr<CiEndpoint> oldOb = is.ci->GetEndpoint(cap->cap);
      GCPtr<CapValue> out = new CapValue(*cap);
      capability tmp = is.ci->AllocEndpoint(bank->cap);
      GCPtr<CiEndpoint> newOb = is.ci->GetEndpoint(tmp);

      memcpy(&newOb->v, &oldOb->v, sizeof(oldOb->v));
      out->cap.u2.oid = tmp.u2.oid;
      out->cap.allocCount = tmp.allocCount;
      return out;
    }
  case ct_Page:
    {
      GCPtr<CiPage> oldOb = is.ci->GetPage(cap->cap);
      GCPtr<CapValue> out = new CapValue(*cap);
      capability tmp = is.ci->AllocPage(bank->cap);
      GCPtr<CiPage> newOb = is.ci->GetPage(tmp);

      memcpy(newOb->data, oldOb->data, oldOb->pgSize);
      out->cap.u2.oid = tmp.u2.oid;
      out->cap.allocCount = tmp.allocCount;
      return out;
    }
  case ct_CapPage:
    {
      GCPtr<CiCapPage> oldOb = is.ci->GetCapPage(cap->cap);
      GCPtr<CapValue> out = new CapValue(*cap);
      capability tmp = is.ci->AllocCapPage(bank->cap);
      GCPtr<CiCapPage> newOb = is.ci->GetCapPage(tmp);

      memcpy(newOb->data, oldOb->data, oldOb->pgSize);
      out->cap.u2.oid = tmp.u2.oid;
      out->cap.allocCount = tmp.allocCount;
      return out;
    }
  case ct_GPT:
    {
      GCPtr<CiGPT> oldOb = is.ci->GetGPT(cap->cap);
      GCPtr<CapValue> out = new CapValue(*cap);
      capability tmp = is.ci->AllocGPT(bank->cap);
      GCPtr<CiGPT> newOb = is.ci->GetGPT(tmp);

      memcpy(&newOb->v, &oldOb->v, sizeof(oldOb->v));
      out->cap.u2.oid = tmp.u2.oid;
      out->cap.allocCount = tmp.allocCount;
      return out;
    }
  case ct_Process:
    {
      GCPtr<CiProcess> oldOb = is.ci->GetProcess(cap->cap);
      GCPtr<CapValue> out = new CapValue(*cap);
      capability tmp = is.ci->AllocProcess(bank->cap);
      GCPtr<CiProcess> newOb = is.ci->GetProcess(tmp);

      memcpy(&newOb->v, &oldOb->v, sizeof(oldOb->v));
      out->cap.u2.oid = tmp.u2.oid;
      out->cap.allocCount = tmp.allocCount;
      return out;
    }

  default:
    is.errStream << is.curAST->loc << " "
		 << pfv.nm
		 << "() requires a non-sender object capability as its "
		 << "argument.\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

GCPtr<Value>
pf_fillpage(PrimFnValue& pfv,
	    InterpState& is,
	    sherpa::CVector<GCPtr<Value> >& args)
{
  needCapArgType(is, pfv.nm, args, 0, ct_Page);
  GCPtr<CapValue> pgCap = args[0].upcast<CapValue>();
  uint32_t value =
    needIntArg(is, pfv.nm, args, 1)->bn.as_uint32();

  if (value > 255) {
    is.errStream << is.curAST->loc << " "
		 << pfv.nm << "() value must be a legal byte value\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  GCPtr<CiPage> pg = is.ci->GetPage(pgCap->cap);

  memset(pg->data, value, is.ci->target.pageSize);

  return &TheUnitValue;
}

GCPtr<Value>
pf_set_page_uint64(PrimFnValue& pfv,
		   InterpState& is,
		   sherpa::CVector<GCPtr<Value> >& args)
{
  needCapArgType(is, pfv.nm, args, 0, ct_Page);
  GCPtr<CapValue> pgCap = args[0].upcast<CapValue>();
  uint32_t addr =
    needIntArg(is, pfv.nm, args, 1)->bn.as_uint32();
  uint64_t value =
    needIntArg(is, pfv.nm, args, 2)->bn.as_uint64();

  size_t slot = addr / sizeof (value);

  if (slot >= (is.ci->target.pageSize / sizeof (value)) ||
      slot * sizeof (value) != addr) {
    is.errStream << is.curAST->loc << " "
		 << pfv.nm << "() addr must be an aligned page address value\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }

  GCPtr<CiPage> pg = is.ci->GetPage(pgCap->cap);

  if (is.ci->target.endian == LITTLE_ENDIAN) {
    size_t idx;
    for (idx = 0; idx < sizeof (value); idx++) {
      pg->data[addr + idx] = (value & (0xff));
      value >>= 8;
    }
  } else {
    size_t idx;
    for (idx = 0; idx < sizeof (value); idx++) {
      pg->data[addr + (sizeof (value) - 1) - idx] = (value & (0xff));
      value >>= 8;
    }
  }

  return &TheUnitValue;
}

GCPtr<Environment<Value> > 
getBuiltinEnv(GCPtr<CoyImage> ci)
{
  if (!builtins) {
    builtins = new Environment<Value>;

    {
      // PRIME BANK INITIALIZATION:

      // Prime bank endpoint is well-known to be endpoint 0. See
      // CoyImage constructor and comment in obstore/CoyImgHdr.h
      GCPtr<CapValue> pbep = new CapValue(ci, ci->CiCap(ct_Endpoint, 0));
      GCPtr<CapValue> pb = new CapValue(*pbep);

      pb->cap.type = ct_Entry;
      pb->cap.u1.protPayload = CoyImage::FullStrengthBank;

      builtins->addConstant("PrimeBankEndpoint", pbep);
      builtins->addConstant("PrimeBank", pb);
    }

    // CONSTRUCTORS FOR CAPABILITIES:

    // Object capability allocators:
    builtins->addConstant("PageCap",
			 new PrimFnValue("PageCap", 1, 1, pf_mk_raw_obcap));

    builtins->addConstant("PhysPageCap",
			 new PrimFnValue("PhysPageCap", 1, 1, pf_mk_raw_obcap));

    // Window capability fabrication:
    builtins->addConstant("Window", 
			 new PrimFnValue("Window", 1, 1, pf_mk_window));

    builtins->addConstant("LocalWindow", 
			 new PrimFnValue("LocalWindow", 2, 2, pf_mk_window));

#if 0
    // Bank fabrication
    builtins->addConstant("make_bank", 
			 new PrimFnValue("make_bank", 1, 1, pf_mk_bank));

    // Cappage fabrication:
    builtins->addConstant("make_cappage", 
			 new PrimFnValue("make_cappage", 1, 1, pf_mk_cappage));

    // Endpoint fabrication:
    builtins->addConstant("make_endpoint", 
			 new PrimFnValue("make_endpoint", 1, 1, 
					 pf_mk_endpoint));

    // GPT fabrication:
    builtins->addConstant("make_gpt", 
			 new PrimFnValue("make_gpt", 1, 1, pf_mk_gpt));

    // Page fabrication:
    builtins->addConstant("make_page", 
			 new PrimFnValue("make_page", 1, 1, pf_mk_page));

    // Process fabrication:
    builtins->addConstant("make_process", 
			 new PrimFnValue("make_process", 1, 1, pf_mk_process));
#endif

    // Sender fabrication:
    builtins->addConstant("enter",
			 new PrimFnValue("enter", 2, 2, pf_mk_entry));

    // AppInt Sender fabrication:
    builtins->addConstant("AppInt",
			 new PrimFnValue("AppInt", 2, 2, pf_mk_appint));

    // Misc capabilities
    builtins->addConstant("NullCap", 
			 new PrimFnValue("NullCap", 0, 0, pf_mk_misccap));
    builtins->addConstant("KeyBits", 
			 new PrimFnValue("KeyBits", 0, 0, pf_mk_misccap));
    builtins->addConstant("Discrim", 
			 new PrimFnValue("Discrim", 0, 0, pf_mk_misccap));
    builtins->addConstant("Range", 
			 new PrimFnValue("Range", 0, 0, pf_mk_misccap));
    builtins->addConstant("Sleep", 
			 new PrimFnValue("Sleep", 0, 0, pf_mk_misccap));
    builtins->addConstant("IrqCtl", 
			 new PrimFnValue("IrqCtl", 0, 0, pf_mk_misccap));
    builtins->addConstant("SchedCtl", 
			 new PrimFnValue("SchedCtl", 0, 0, pf_mk_misccap));
    builtins->addConstant("Checkpoint", 
			 new PrimFnValue("Checkpoint", 0, 0, pf_mk_misccap));
    builtins->addConstant("ObStore", 
			 new PrimFnValue("ObStore", 0, 0, pf_mk_misccap));
    builtins->addConstant("IoPerm", 
			 new PrimFnValue("IoPerm", 0, 0, pf_mk_misccap));
    builtins->addConstant("PinCtl", 
			 new PrimFnValue("PinCtl", 0, 0, pf_mk_misccap));
    builtins->addConstant("SysCtl", 
			 new PrimFnValue("SysCtl", 0, 0, pf_mk_misccap));
    builtins->addConstant("KernLog", 
			 new PrimFnValue("KernLog", 0, 0, pf_mk_misccap));

    // CAPABILITY TRANSFORMERS:

    builtins->addConstant("readonly", 
			 new PrimFnValue("readonly", 1, 1, pf_downgrade));
    builtins->addConstant("weaken", 
			 new PrimFnValue("weaken", 1, 1, pf_downgrade));
    builtins->addConstant("noexec", 
			 new PrimFnValue("noexec", 1, 1, pf_downgrade));

    builtins->addConstant("opaque", 
			 new PrimFnValue("opaque", 1, 1, pf_downgrade));

    builtins->addConstant("nocall", 
			 new PrimFnValue("nocall", 1, 1, pf_downgrade));

    builtins->addConstant("nocache", 
			 new PrimFnValue("nocache", 1, 1, pf_downgrade));

#if 0
    builtins->addConstant("writethrough", 
			 new PrimFnValue("writethrough", 1, 1, pf_downgrade));
#endif

    builtins->addConstant("guard", 
			 new PrimFnValue("guard", 2, 3, pf_guard_cap));

    // CAPABILITY ACCESSORS:

    builtins->addConstant("get_l2g", 
			 new PrimFnValue("get_l2g", 1, 1, pf_memcap_ops));
    builtins->addConstant("get_match", 
			 new PrimFnValue("get_match", 1, 1, pf_memcap_ops));

    // UTILITY FUNCTIONS

    builtins->addConstant("fatal", 
			 new PrimFnValue("fatal", 1, 0, pf_fatal));

    builtins->addConstant("doprint", 
			 new PrimFnValue("doprint", 1, 0, pf_doprint));

    builtins->addConstant("print_space", 
			 new PrimFnValue("print_space", 1, 1, pf_doprint_space));

    builtins->addConstant("print_tree", 
			 new PrimFnValue("print_tree", 1, 1, pf_doprint_tree));

    builtins->addConstant("guarded_space", 
			 new PrimFnValue("guarded_space", 3, 4, pf_guarded_space));

    builtins->addConstant("insert_subspace", 
			 new PrimFnValue("insert_subspace", 4, 5, pf_insert_subspace));

    builtins->addConstant("lookup_addr",
			  new PrimFnValue("lookup_addr", 2, 2, pf_lookup_addr));
    builtins->addConstant("readfile", 
			 new PrimFnValue("readfile", 2, 3, pf_readfile));

    builtins->addConstant("loadimage", 
			 new PrimFnValue("loadimage", 2, 2, pf_loadimage));

    builtins->addConstant("dup", 
			 new PrimFnValue("dup", 2, 2, pf_dup));

    // Page content manipulation
    builtins->addConstant("fillpage",
			 new PrimFnValue("fillpage", 2, 2, pf_fillpage));

    builtins->addConstant("set_page_uint64",
			 new PrimFnValue("set_page_uint64", 3, 3, 
					 pf_set_page_uint64));

    {
      // Environment for storage object constructor capabilities:
      GCPtr<Environment<Value> > ctors = new Environment<Value>;

      // Bank fabrication
      ctors->addConstant("Bank", 
			 new PrimFnValue("make_bank", 1, 1, pf_mk_bank));

      // Cappage fabrication:
      ctors->addConstant("CapPage", 
			 new PrimFnValue("make_cappage", 1, 1, pf_mk_cappage));

      // GPT fabrication:
      ctors->addConstant("GPT", 
			 new PrimFnValue("make_gpt", 1, 1, pf_mk_gpt));

      // Endpoint fabrication:
      ctors->addConstant("Endpoint", 
			 new PrimFnValue("make_endpoint", 1, 1, 
					 pf_mk_endpoint));

      // Page fabrication:
      ctors->addConstant("Page", 
			 new PrimFnValue("make_page", 1, 1, pf_mk_page));

      // Process fabrication:
      ctors->addConstant("Process", 
			 new PrimFnValue("make_process", 1, 1, pf_mk_process));



      builtins->addConstant("#new", new EnvValue(ctors));
    }

#if 0
    {
      // Environment for miscellaneous capabilities:
      GCPtr<Environment<Value> > caps = new Environment<Value>;

      caps->addConstant("null", new CapValue(ci, ci->CiCap()));
      caps->addConstant("keybits", new CapValue(ci, ci->CiCap(ct_KeyBits)));
      caps->addConstant("discrim", new CapValue(ci, ci->CiCap(ct_Discrim)));
      caps->addConstant("range", new CapValue(ci, ci->CiCap(ct_Range)));
      caps->addConstant("sleep", new CapValue(ci, ci->CiCap(ct_Sleep)));
      caps->addConstant("irqctl", new CapValue(ci, ci->CiCap(ct_IRQCtl)));
      caps->addConstant("schedctl", new CapValue(ci, ci->CiCap(ct_SchedCtl)));
      caps->addConstant("checkpoint", new CapValue(ci, ci->CiCap(ct_Checkpoint)));
      caps->addConstant("obstore", new CapValue(ci, ci->CiCap(ct_ObStore)));
      caps->addConstant("ioperm", new CapValue(ci, ci->CiCap(ct_IoPerm)));
      caps->addConstant("pinctl", new CapValue(ci, ci->CiCap(ct_PinCtl)));

      builtins->addConstant("coyotos", new EnvValue(caps));
    }
#endif

    // Arithmetic and comparison operators:

    builtins->addPureConstant("+", 
			      new PrimFnValue("+", 1, 0, pf_add));
    builtins->addPureConstant("-", 
			      new PrimFnValue("-", 1, 0, pf_add));
    builtins->addPureConstant("*", 
			      new PrimFnValue("*", 1, 0, pf_mul));
    builtins->addPureConstant("/", 
			      new PrimFnValue("/", 1, 0, pf_div));
    builtins->addPureConstant("%", 
			      new PrimFnValue("%", 1, 0, pf_mod));
    builtins->addPureConstant("unary-", 
			      new PrimFnValue("-", 1, 0, pf_negate));

    builtins->addPureConstant("<", 
			      new PrimFnValue("<", 1, 0, pf_cmp));
    builtins->addPureConstant(">", 
			      new PrimFnValue(">", 1, 0, pf_cmp));

    builtins->addPureConstant("<=", 
			      new PrimFnValue("<=", 1, 0, pf_cmpe));
    builtins->addPureConstant(">=", 
			      new PrimFnValue(">=", 1, 0, pf_cmpe));
    builtins->addPureConstant("==", 
			      new PrimFnValue("==", 1, 0, pf_cmpe));
    builtins->addPureConstant("!=", 
			      new PrimFnValue("!=", 1, 0, pf_cmpe));
    builtins->addPureConstant("!", 
			      new PrimFnValue("!", 1, 0, pf_not));

  }

  return builtins;
}
