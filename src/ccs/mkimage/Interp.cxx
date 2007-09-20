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

#include "UocInfo.hxx"
#include "builtin.hxx"

// #define DEBUG

using namespace std;
using namespace sherpa;

struct FnReturn {
  GCPtr<Value> val;

  FnReturn(GCPtr<Value> _val)
  {
    val = _val;
  }

  virtual ~FnReturn();
};

FnReturn::~FnReturn()
{
}

static bool
isValue(GCPtr<Value> v)
{
  if (v->kind == Value::vk_unit)
    return false;
  else if (v->kind == Value::vk_primField) {
    GCPtr<PrimFieldValue> pfv = v.upcast<PrimFieldValue>();
    if (pfv->isVector && pfv->ndx == ~0u)
      return false;
  }

  return true;
}

static bool
canDereference(GCPtr<Value> v)
{
  if (v->kind == Value::vk_primField) {
    GCPtr<PrimFieldValue> pfv = v.upcast<PrimFieldValue>();

    if (!pfv->isVector)
      return false;

    if (pfv->ndx != ~0u)		// already dereferenced
      return false;

    return true;
  }

  if (v->kind == Value::vk_cap) {
    GCPtr<CapValue> cv = v.upcast<CapValue>();
    if (cv->cap.type == ct_CapPage || cv->cap.type == ct_Page)
      return true;
  }

  return false;
}

GCPtr<Value>
AST::interp(InterpState& is)
{
  is.curAST = this;

  switch(astType) {
  case at_intLiteral:
  case at_stringLiteral:
    return litValue;

  case at_uoc:
    {
      GCPtr<Value> v = &TheUnitValue;

      // We may be executing in pure mode. If so, the only children we
      // should process are imports and enum ASTs. The rest should be
      // skipped. In future we may add puredef, so use an accessor on
      // the AST to determine what is pure.
      if (is.pureMode) {
	for(size_t c = 1; c < children->size(); c++)
	  if (child(c)->isPureAST())
	    v = child(c)->interp(is);
      }
      else {
	for(size_t c = 1; c < children->size(); c++)
	    v = child(c)->interp(is);
      }
      return v;
    }

  case at_s_import:
    {
      std::string ident = child(0)->s;
      std::string modName = child(1)->s;

      GCPtr<Binding<Value> > b = is.env->getLocalBinding(ident);
      if (b) {
	is.errStream << loc << " "
		  << "Symbol \""
		  << ident << "\" is already bound.\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      // It seems regrettable that this step abandons the interpreter
      // stack, because we cannot, in consequence, print the chain of
      // imports when it turns out to be circular. In the main, we
      // abandon the interpreter stack because to do otherwise we
      // would need to invade the UocInfo::compile() logic rather
      // deeply.
      //
      // For the moment, I don't think that it is worthwhile to do
      // that. It is easier to maintain the separate stack of
      // import records for that purpose if we decided that we need
      // to, and that is what I have actually done.
      GCPtr<UocInfo> uoc = 
	UocInfo::importModule(is.errStream, loc, is.ci, modName, 
			      is.pureMode ? CV_CONSTDEF : CV_INTERP);

      is.env->addBinding(ident, new EnvValue(uoc->getExportedEnvironment()));

      return &TheUnitValue;
    }

  case at_ifelse:
    {
      GCPtr<Value> vcond = 
	child(0)->interp(is)->get();

      bool cond;

      if (vcond->kind == Value::vk_int) {
	GCPtr<IntValue> a = vcond.upcast<IntValue>();
	cond = (a->bn != 0);
      }
      else if (vcond->kind == Value::vk_float) {
	GCPtr<FloatValue> a = vcond.upcast<FloatValue>();
	cond = (a->d != 0.0);
      }
      else if (vcond->kind == Value::vk_string) {
	cond = true;
      }
      else {
	is.errStream << loc << " "
		  << "Inappropriate dynamic types to operator +\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }
      
      if (cond)
	return child(1)->interp(is);

      // Else process 'else' clause if present:
      if (children->size() == 3)
	return child(2)->interp(is);

      // Else return unit:
      return &TheUnitValue;
    }

  case at_fncall:
    {
      GCPtr<Value> fn = child(0)->interp(is)->get();

      /* We may be in pure mode if we are on the RHS of an enum
	 definition. In that case, only pure vk_primfn's are currently
	 permitted. */
      if (fn->kind == Value::vk_function) {
	GCPtr<FnValue> fnv = fn.upcast<FnValue>();

	GCPtr<AST> args = fnv->args;
	GCPtr<AST> body = fnv->body;

	if (args->children->size() + 1 != children->size()) {
	  is.errStream << loc << " "
		    << "Wrong number of arguments to \""
		    << fnv->nm << "\"\n";
	  THROW(excpt::BadValue, "Bad interpreter result");
	}

	GCPtr<Environment<Value> > closedEnv = fnv->closedEnv;
	GCPtr<Environment<Value> > argEnv = new Environment<Value>(closedEnv);
	GCPtr<Environment<Value> > bodyEnv = new Environment<Value>(argEnv);

	for (size_t c = 0; c < args->children->size(); c++) {
	  std::string argName = args->child(c)->s;
	  GCPtr<Value> v = child(c+1)->interp(is);
	  argEnv->addBinding(argName, v->get());
	}
      
	GCPtr<Value> result;

	try {
	  InterpState bodyState(is);
	  bodyState.env = bodyEnv;
	  result = body->interp(bodyState);
	}
	catch (FnReturn ir) {
	  result = ir.val;
	}
	catch (...) {
	  throw;
	}

	return result->get();
      }
      else if (fn->kind == Value::vk_primfn) {
	GCPtr<PrimFnValue> fnv = fn.upcast<PrimFnValue>();

	size_t nArgs = children->size() - 1;
	if (nArgs < fnv->minArgs) {
	  is.errStream << loc << " "
		    << "Not enough arguments to \""
		    << fnv->nm << "\"\n";
	  THROW(excpt::BadValue, "Bad interpreter result");
	}
	if (fnv->maxArgs && nArgs > fnv->maxArgs) {
	  is.errStream << loc << " "
		    << "Too many arguments to \""
		    << fnv->nm << "\"\n";
	  THROW(excpt::BadValue, "Bad interpreter result");
	}

	CVector<GCPtr<Value> > args;

	for (size_t c = 1; c < children->size(); c++) {
	  GCPtr<Value> v = child(c)->interp(is);
	  args.append(v->get());
	}
      
	GCPtr<Value> result = fnv->fn(*fnv, is, args);
	return result->get();
      }
      else if (fn->kind == Value::vk_primMethod) {
	GCPtr<PrimMethodValue> pmv = fn.upcast<PrimMethodValue>();

	if (is.pureMode) {
	  is.errStream << loc << " " 
		       << "Illegal call to non-pure method \""
		       << pmv->nm << "\" on RHS of enum definition.\n";
	  THROW(excpt::BadValue, "Bad interpreter result");
	}

	size_t nArgs = children->size() - 1;
	if (nArgs < pmv->minArgs) {
	  is.errStream << loc << " "
		    << "Not enough arguments to \""
		    << pmv->nm << "\"\n";
	  THROW(excpt::BadValue, "Bad interpreter result");
	}
	if (pmv->maxArgs && nArgs > pmv->maxArgs) {
	  is.errStream << loc << " "
		    << "Too many arguments to \""
		    << pmv->nm << "\"\n";
	  THROW(excpt::BadValue, "Bad interpreter result");
	}

	CVector<GCPtr<Value> > args;

	for (size_t c = 1; c < children->size(); c++) {
	  GCPtr<Value> v = child(c)->interp(is);
	  args.append(v->get());
	}
      
	GCPtr<Value> result = pmv->fn(*pmv, is, args);
	return result->get();
      }
      else {
	is.errStream << loc << " "
		  << "Call using non-function value.\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }
    }

#if 0
  case at_s_progdef:
    {
      // A program definition defines a new first-class environment

      // No need to check types on the first two, because parser
      // ensures they are right:
      std::string ident = child(0)->s;
      std::string exeName = child(1)->s;

      GCPtr<Binding<Value> > b = is.env->getLocalBinding(ident);
      if (b) {
	is.errStream << loc << " "
		  << "Identifier \""
		  << ident << "\" is already bound.\n";
<	THROW(excpt::BadValue, "Bad interpreter result");
      }

      ExecImage ei(exeName);

      GCPtr<Environment<Value> > progEnv = new Environment<Value>();
      GCPtr<Environment<Value> > toolNdxEnv = new Environment<Value>();
      GCPtr<CapValue> toolPageCap = 
	new CapValue(is.ci, is.ci->AllocCapPage(is.curBank));
      GCPtr<CiCapPage> toolPage = is.ci->GetCapPage(toolPageCap->cap);

      GCPtr<EnvValue> progEnvValue = new EnvValue(progEnv);

      is.env->addBinding(ident, progEnvValue);
      progEnv->addBinding("pc", new IntValue(ei.entryPoint));
      progEnv->addBinding("tool", new EnvValue(toolNdxEnv));
      progEnv->addBinding("toolPage", toolPageCap);

      progEnv->setFlags("pc", BF_CONSTANT);
      progEnv->setFlags("tool", BF_CONSTANT);
      progEnv->setFlags("toolPage", BF_CONSTANT);

      capability addrSpace = is.ci->CiCap();

      for (size_t i = 0; i < ei.region.size(); i++) {
	ExecRegion& er = ei.region[i];

	for (uint64_t pos = 0; pos < er.filesz; pos += is.ci->target.pageSize) {
	  size_t nBytes = min(er.filesz - pos, is.ci->target.pageSize);

	  capability pgCap = is.ci->AllocPage(is.curBank);
	  GCPtr<CiPage> pg = is.ci->GetPage(pgCap);
	  memcpy(pg->data, ei.image.substr(er.offset + pos, nBytes).c_str(), nBytes);

	  if ((er.perm & ER_X) == 0)
	    pgCap.nx = 1;
	  if ((er.perm & ER_W) == 0)
	    pgCap.ro = 1;

#ifdef DEBUG
	  std::ios_base::fmtflags flgs = std::cerr.flags();

	  std::cerr << "Calling addSubspace at " 
		    << std::hex
		    << std::showbase
		    << er.vaddr + pos
		    << std::dec
		    << std::endl;

	  std::cerr.flags(flgs);

	  std::cerr << "  Space    = " << addrSpace << endl;
	  std::cerr << "  SubSpace = " << pgCap << endl;
#endif

	  addrSpace = 
	    is.ci->AddSubSpaceToSpace(bank, addrSpace, er.vaddr + pos, pgCap);
#ifdef DEBUG
	  std::cerr << "  Result   = " << addrSpace << endl;
#endif
	}
      }

      progEnv->addBinding("space", new CapValue(is.ci, addrSpace));
      progEnv->setFlags("space", BF_CONSTANT);


      for (size_t ndx = 2; ndx < children->size(); ndx++) {
	GCPtr<AST> toolAST = child(ndx);
	
	std::string toolIdent = toolAST->child(0)->s;
	GCPtr<Value> toolNdx = toolAST->child(1)->interp(is)->get();
	GCPtr<Value> toolVal = toolAST->child(2)->interp(is)->get();

	if (toolNdx->kind != Value::vk_int) {
	  is.errStream << toolAST->child(1)->loc << " " << 
	    "Tool indices must be integers." << '\n';
	  THROW(excpt::BadValue, "Bad interpreter result");
	}
	if (toolVal->kind != Value::vk_cap) {
	  is.errStream << toolAST->child(2)->loc << " " << 
	    "Tool values must be capabilities." << '\n';
	  THROW(excpt::BadValue, "Bad interpreter result");
	}

	size_t nSlots = toolPage->pgSize/sizeof(capability);

	if (toolNdx.upcast<IntValue>()->bn >= nSlots) {
	  is.errStream << toolAST->child(1)->loc << " " << 
	    "Tool index must be less than number of CapPage slots ("
		       << toolPage->pgSize
		       << ")\n";
	  THROW(excpt::BadValue, "Bad interpreter result");
	}

	size_t capPgNdx = toolNdx.upcast<IntValue>()->bn.as_uint32();

	toolNdxEnv->addBinding(toolIdent, toolNdx);
	toolNdxEnv->setFlags(toolIdent, BF_CONSTANT);
	toolPage->data[capPgNdx] = toolVal.upcast<CapValue>()->cap;
      }

      return progEnvValue;
    }
#endif

  case at_s_assign:
    {
      GCPtr<Value> vdest = child(0)->interp(is);
      GCPtr<Value> v = child(1)->interp(is)->get();

      if (!isValue(v)) {
	is.errStream << loc << " " << 
	  "Right hand side of assignment is not a value." << '\n';
	  THROW(excpt::BadValue, "Bad interpreter result");
      }

      try {
	vdest->set(v->get());
      }
#if 1
      catch (const UException<&excpt::BoundsError>& bv) {
	is.errStream << loc << " "
		  << "Illegal assignment (array bounds violation)\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }
      catch (const UException<&excpt::BadValue>& bv) {
	is.errStream << loc << " "
		  << "Illegal assignment (bad RHS type)\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }
      catch (const UException<&excpt::NoAccess>& bv) {
	is.errStream << loc << " "
		  << "Illegal assignment (LHS is constant)\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }
#endif
      catch (...) {
	is.errStream << loc << " "
		  << "Unknown error\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      return &TheUnitValue;
    }

  case at_s_export_def:
  case at_s_def:
    {
      GCPtr<Value> v = child(1)->interp(is);
      std::string ident = child(0)->s;

      if (!isValue(v)) {
	is.errStream << loc << " " << 
	  "Right hand side of assignment is not a value." << '\n';
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      GCPtr<Binding<Value> > b = is.env->getLocalBinding(ident);
      if (b) {
	is.errStream << loc << " "
		  << "Variable \""
		  << ident << "\" is already bound.\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      is.env->addBinding(ident, v->get());
      if (astType == at_s_export_def)
	is.env->setFlags(ident, BF_EXPORTED);
      
      return &TheUnitValue;
    }

  case at_s_export_enum:
  case at_s_enum:
  case at_s_export_capreg:
  case at_s_capreg:
    {
      BigNum curValue = 0;

      GCPtr<Environment<Value> > env;
      if (child(0)->astType == at_ident) {
	env = new Environment<Value>();
	std::string ident = child(0)->s;

	GCPtr<Binding<Value> > b = is.env->getLocalBinding(ident);
	if (b) {
	  is.errStream << loc << " "
		       << "Symbol \""
		       << ident << "\" is already bound.\n";
	  THROW(excpt::BadValue, "Bad interpreter result");
	}

	is.env->addBinding(ident, new EnvValue(env));
	if (astType == at_s_export_enum || astType == at_s_export_capreg)
	  is.env->setFlags(ident, BF_EXPORTED);
      }
      else
	env = is.env;

      for (size_t i = 1; i < children->size(); i++) {
	GCPtr<AST> eBinding = child(i);

	std::string ident = eBinding->child(0)->s;

	GCPtr<Binding<Value> > b = env->getLocalBinding(ident);
	if (b) {
	  is.errStream << loc << " "
		       << "Identifier \""
		       << ident << "\" is already bound.\n";
	  THROW(excpt::BadValue, "Bad interpreter result");
	}

	if (eBinding->children->size() == 2) {
	  InterpState exprInterpState = is;
	  exprInterpState.pureMode = true;
	  GCPtr<Value> v = eBinding->child(1)->interp(exprInterpState)->get();
	  if (v->kind != Value::vk_int) {
	    is.errStream << eBinding->child(1)->loc << " "
			 << "Enum/capreg value must be an integer.\n";
	    THROW(excpt::BadValue, "Bad interpreter result");
	  }

	  curValue = v.upcast<IntValue>()->bn;
	}

	env->addBinding(ident, new IntValue(curValue));
	env->setFlags(ident, BF_PURE);
	if (astType == at_s_export_enum || astType == at_s_export_capreg)
	  env->setFlags(ident, BF_EXPORTED);

	curValue = curValue + 1;
      }

      return &TheUnitValue;
    }

#if 0
  case at_s_constitdef:
    {
      std::string ident = child(0)->s;

      GCPtr<Binding<Value> > b = env->getLocalBinding(ident);
      if (b) {
	is.errStream << loc << " "
		  << "Identifier \""
		  << ident << "\" is already bound.\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      GCPtr<Environment<Value> > newEnv = new Environment<Value>(env);
      GCPtr<AST> ids = child(1);

      for (size_t i = 0; i < ids->children->size(); i++) {
	std::string ident = ids->child(i)->s;
	newEnv->addBinding(ident, new IntValue(i));
	newEnv->setFlags(ident, BF_CONSTANT);
      }

      env->addBinding(ident, new EnvValue(newEnv));

      return &TheUnitValue;
    }
#endif

  case at_s_export_fndef:
  case at_s_fndef:
    {
      // All we do here is enter it into the environment as a value,
      // much as if we were processing s_bind

      // Check for a competing binding:
      std::string ident = child(0)->s;
      GCPtr<AST> args = child(1);
      GCPtr<AST> body = child(2);

      GCPtr<Binding<Value> > b = is.env->getLocalBinding(ident);
      if (b) {
	is.errStream << loc << " "
		  << "Variable \""
		  << ident << "\" is already bound.\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      is.env->addBinding(ident, new FnValue(ident, is.env, args, body));
      if (astType == at_s_export_fndef)
	is.env->setFlags(ident, BF_EXPORTED);

      return &TheUnitValue;
    }

  case at_ident:
    {
      GCPtr<Binding<Value> > b = is.env->getBinding(s);
      if (!b) {
	is.errStream << loc << " "
		  << "Reference to unbound variable \""
		  << s << "\"\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      if (is.pureMode && !(b->flags & BF_PURE)) {
	is.errStream << loc << " "
		     << "Reference to impure binding \""
		     << s << "\" in ENUM definition\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      return new BoundValue(b);
    }

  case at_dot:
    {
      /* If this appears in context of an ENUM RHS, then we are in
	 pure mode. Given lhs.rhs, the pure mode identifier restriction
	 applies only to the 'rhs'. The lhs is processed without
	 regard to pure mode. */
      InterpState lhsInterpState = is;
      lhsInterpState.pureMode = false;

      // This case is going to end up being quite a nuisance in due
      // course, but for now.
      GCPtr<Value> vl = child(0)->interp(lhsInterpState)->get();

      if (child(1)->astType != at_ident) {
	is.errStream << loc << " "
		  << "Right-hand side of '.' must be an identifier.\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      GCPtr<Environment<Value> > env = vl->getEnvironment();

      if (!env) {
	is.errStream << loc << " "
		  << "Cannot handle '.' on values without environments.\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      GCPtr<Binding<Value> > binding = env->getBinding(child(1)->s);

      if (!binding) {
	is.errStream << loc << " "
		  << "Reference to unbound field/member \""
		     << child(1)->s << "\"\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      if (is.pureMode && !(binding->flags & BF_PURE)) {
	is.errStream << loc << " "
		     << "Reference to impure binding \""
		     << child(1)->s << "\" in ENUM definition\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      GCPtr<Value> val = binding->val;

      if (val->isOpen)		// container was not an environment
	return val.upcast<OpenValue>()->closeWith(vl->get());

      return new BoundValue(binding);

    }

  case at_vecref:
    {
      // There are actually two distinct cases here: dereferencing an
      // actual vector or vector-like LHS vs. dereferencing a field
      // that merely *behaves* like a vector. The latter case arises
      // in things like procCap.capReg[5]. That is the vk_primField
      // case.

      GCPtr<Value> v_vec = child(0)->interp(is);
      GCPtr<Value> v_ndx = child(1)->interp(is)->get();

      // This code should not currently be used. The idea is that if
      // we see someGpt.cap[5][6], then someGpt.cap[5] is probably a
      // capPage. In any case we need to treat the someGpt.cap[5] as
      // an r-value and then deal with the dereference of that.
      if (v_vec->kind == Value::vk_primField) {
	GCPtr<PrimFieldValue> pfv = v_vec.upcast<PrimFieldValue>();
	if (pfv->ndx != ~0u)
	  v_vec = pfv->get();
      }
      else if (v_vec->kind == Value::vk_boundValue)
	v_vec = v_vec->get();

      if (!canDereference(v_vec)) {
	is.errStream << loc << " "
		  << "LHS of index operation must be dereferenceable.\n";
	THROW(excpt::BadValue, "Bad interpreter result");
      }

      size_t ndx = v_ndx.upcast<IntValue>()->bn.as_uint32();

      if (v_vec->kind == Value::vk_primField) {
	GCPtr<PrimFieldValue> pfv = v_vec.upcast<PrimFieldValue>();

	pfv->ndx = ndx;
	pfv->cacheTheValue();	// it's now a value r-value
	return pfv;
      }
      else if (v_vec->kind == Value::vk_cap)
	return v_vec.upcast<CapValue>()->derefAsVector(ndx);

      is.errStream << loc << " "
		<< "mkimage bug: Inappropriate type on LHS "
		<< "of index operation.\n";
      THROW(excpt::BadValue, "Bad interpreter result");
    }

  case at_s_print:
    {

      GCPtr<Value> v0 = 
	child(0)->interp(is)->get();
      CVector<GCPtr<Value> > args;
      args.append(v0);

      pf_print_one(is, v0);
      std::cout << '\n';

      return &TheUnitValue;
    }

  case at_s_printstar:
    {
      GCPtr<Value> v0 = 
	child(0)->interp(is)->get();
      CVector<GCPtr<Value> > args;
      args.append(v0);

      return pf_doprint_star(is, args);
    }

  case at_s_return:
    {
      GCPtr<Value> v0 = child(0)->interp(is);

      throw FnReturn(v0);
    }

  case at_block:
    {
      InterpState blockState(is);
      blockState.env = new Environment<Value>(is.env);
      GCPtr<Value> v;

      for (size_t c = 0; c < children->size(); c++)
	v = child(c)->interp(blockState);

      return v->get();
    }

  default:
    is.errStream << loc << " "
	      << "Do not know how to interpret AST nodes of type \""
	      << astName()
	      << "\"\n";
    THROW(excpt::BadValue, "Bad interpreter result");
  }
}

bool
UocInfo::pass_interp(std::ostream& errStream, unsigned long flags)
{
  GCPtr<AST> modName = ast->child(0);
  if (uocName != "coyotos.TargetInfo" && modName->s != uocName) {
    errStream << modName->loc << " "
	      << "Defined module name \"" << modName->s 
	      << "\" does not match file module name \"" << uocName << "\"."
	      << "\n";
    return false;
  }

  env = new Environment<Value>(getBuiltinEnv(ci));

  InterpState is(errStream, ci, ast, env);

  try {
    ast->interp(is);
  } catch (...) {
    // Diagnostic has already been issued.
    return false;
  }

  return true;
}

InterpState::InterpState(std::ostream & _errStream, 
			 sherpa::GCPtr<CoyImage> _ci,
			 sherpa::GCPtr<AST> _curAST,
			 sherpa::GCPtr<Environment<Value> > _env
			 )
  : errStream(_errStream)
{
  ci = _ci;
  curAST = _curAST;
  env = _env;
  pureMode = false;
}

InterpState::InterpState(const InterpState& is)
  : errStream(is.errStream)
{
  ci = is.ci;
  curAST = is.curAST;
  env = is.env;
  pureMode = is.pureMode;
}
