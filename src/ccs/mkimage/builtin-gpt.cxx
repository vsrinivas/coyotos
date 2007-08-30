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

#include <libsherpa/utf8.hxx>

#include "Environment.hxx"
#include "Value.hxx"
#include "AST.hxx"

using namespace sherpa;

static GCPtr<Environment<Value> > gptEnv = 0;

// Set field values:
static void 
GptSet(PrimFieldValue& pfv,
	   GCPtr<Value> & val)
{
  GCPtr<CapValue> thisv =  pfv.thisValue.upcast<CapValue>();
  GCPtr<CoyImage> ci = thisv->ci;

  GCPtr<CiGPT> ob = ci->GetGPT(thisv->cap);

#if 0
  if (pfv.ident == "recipient") {
    if (val->kind != Value::vk_cap)
      throw excpt::BadValue;

    ob->v.state.recipient = val.upcast<CapValue>()->cap;
    return;
  }
#endif

  if (pfv.ident == "l2v") {
    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.l2v = val.upcast<IntValue>()->bn.as_uint32();
    return;
  }

  if (pfv.ident == "bg") {
    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.bg = (val.upcast<IntValue>()->bn == 0) ? 0 : 1;
    return;
  }

  if (pfv.ident == "ha") {
    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.ha = (val.upcast<IntValue>()->bn == 0) ? 0 : 1;
    return;
  }

  if (pfv.ident == "cap") {
    if (pfv.ndx == ~0u || pfv.ndx >= NUM_GPT_SLOTS) {
      // Cannot set() an un-dereferenced vector or exceed the bound
      throw excpt::BadValue;
    }

    ob->v.cap[pfv.ndx] = val.upcast<CapValue>()->cap;
    return;
  }

  assert(false && "Missing or unknown field name");
}

// Get field values:
static GCPtr<Value> 
GptGet(PrimFieldValue& pfv)
{
  // Only field is "cap", so no need to bother with dispatching:
  
  GCPtr<CapValue> thisv =  pfv.thisValue.upcast<CapValue>();
  GCPtr<CoyImage> ci = thisv->ci;

  GCPtr<CiGPT> ob = ci->GetGPT(thisv->cap);

#if 0
  if (pfv.ident == "recipient")
    return new CapValue(ci, ob->v.recipient);
#endif

  if (pfv.ident == "l2v")
    return new IntValue(ob->v.l2v);

  if (pfv.ident == "ha")
    return new IntValue(ob->v.ha);

  if (pfv.ident == "bg")
    return new IntValue(ob->v.bg);

  if (pfv.ident == "cap") {
    if (pfv.ndx == ~0u || pfv.ndx >= NUM_GPT_SLOTS) {
      // Cannot get() an un-dereferenced vector or exceed the bound
      throw excpt::BadValue;
    }

    return new CapValue(ci, ob->v.cap[pfv.ndx]);
  }

  assert(false && "Missing or unknown field name");
}

GCPtr<Environment<Value> > 
CapValue::getGptEnvironment()
{
  if (!gptEnv) {
    gptEnv = new Environment<Value>;

#if 0
    gptEnv->addBinding("endpointID", 
			   new PrimFieldValue("endpointID", false,
					      GptSet, 
					      GptGet));

    gptEnv->addBinding("recipient", 
			   new PrimFieldValue("recipient", false,
					      GptSet, 
					      GptGet));
#endif

    gptEnv->addBinding("cap", 
			   new PrimFieldValue("cap", false,
					      GptSet, 
					      GptGet,
					      true));

    gptEnv->addBinding("bg", 
			   new PrimFieldValue("bg", false,
					      GptSet, 
					      GptGet));

    gptEnv->addBinding("ha", 
			   new PrimFieldValue("ha", false,
					      GptSet, 
					      GptGet));
    gptEnv->addBinding("l2v", 
			   new PrimFieldValue("l2v", false,
					      GptSet, 
					      GptGet));
  }

  return gptEnv;
}
