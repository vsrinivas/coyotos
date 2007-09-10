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

static GCPtr<Environment<Value> > endpointEnv = 0;

// Set field values:
static void 
EndpointSet(PrimFieldValue& pfv,
	   GCPtr<Value> & val)
{
  GCPtr<CapValue> thisv =  pfv.thisValue.upcast<CapValue>();
  GCPtr<CoyImage> ci = thisv->ci;

  GCPtr<CiEndpoint> ob = ci->GetEndpoint(thisv->cap);

  if (pfv.ident == "protPayload") {
    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.protPayload = val.upcast<IntValue>()->bn.as_uint32();
    return;
  }

  if (pfv.ident == "endpointID") {
    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.endpointID = val.upcast<IntValue>()->bn.as_uint64();
    return;
  }

  if (pfv.ident == "recipient") {
    if (val->kind != Value::vk_cap)
      throw excpt::BadValue;

    ob->v.recipient = val.upcast<CapValue>()->cap;
    return;
  }

  if (pfv.ident == "pm") {
    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.pm = (val.upcast<IntValue>()->bn == 0) ? 0 : 1;
    return;
  }

  assert(false && "Missing or unknown field name");
}

// Get field values:
static GCPtr<Value> 
EndpointGet(PrimFieldValue& pfv)
{
  // Only field is "cap", so no need to bother with dispatching:
  
  GCPtr<CapValue> thisv =  pfv.thisValue.upcast<CapValue>();
  GCPtr<CoyImage> ci = thisv->ci;

  GCPtr<CiEndpoint> ob = ci->GetEndpoint(thisv->cap);

  if (pfv.ident == "protPayload")
    return new IntValue(ob->v.protPayload);

  if (pfv.ident == "endpointID")
    return new IntValue(ob->v.endpointID);

  if (pfv.ident == "recipient")
    return new CapValue(ci, ob->v.recipient);

  if (pfv.ident == "pm")
    return new IntValue(ob->v.pm);

  assert(false && "Missing or unknown field name");
}

#if 0
// Endpoint methods:
static GCPtr<Value> 
EndpointMethod(PrimMethodValue& pmv,
	   std::ostream&,
	   sherpa::GCPtr<AST> callSite,
	   sherpa::CVector<sherpa::GCPtr<Value> >& args)
{
  // Only field is "cap", so no need to bother with dispatching:
  
  GCPtr<CapValue> thisv =  pmv.thisValue.upcast<CapValue>();
  GCPtr<CoyImage> ci = thisv->ci;

  GCPtr<CiEndpoint> ob = ci->GetEndpoint(thisv->cap);

  if (pmv.nm == "setProcess") {
    GCPtr<Value> val = args[0];

    if (val->kind != Value::vk_cap)
      throw excpt::BadValue;

    ob->v.recipient = val.upcast<CapValue>()->cap;
    return &TheUnitValue;
  }

  if (pmv.nm == "setPayloadMatch") {
    GCPtr<Value> val = args[0];

    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.pm = (val.upcast<IntValue>()->bn == 0) ? 0 : 1;
    return &TheUnitValue;
  }

  if (pmv.nm == "setSqueak") {
    GCPtr<Value> val = args[0];

    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.sq = (val.upcast<IntValue>()->bn == 0) ? 0 : 1;
    return &TheUnitValue;
  }

  if (pmv.nm == "setEndpointID") {
    GCPtr<Value> val = args[0];

    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.endpointID = val.upcast<IntValue>()->bn.as_uint64();
    return &TheUnitValue;
  }

  if (pmv.nm == "makeSendCap") {
    GCPtr<CapValue> cv = new CapValue(ci, thisv->cap);

    cv->cap.type = ct_EndpointSend;

    return cv;
  }

  assert(false && "Missing or unknown field name");
}
#endif

GCPtr<Environment<Value> > 
CapValue::getEndpointEnvironment()
{
  if (!endpointEnv) {
    endpointEnv = new Environment<Value>;

    endpointEnv->addBinding("protPayload", 
			   new PrimFieldValue("protPayload", false,
					      EndpointSet, 
					      EndpointGet));
    endpointEnv->addBinding("endpointID", 
			   new PrimFieldValue("endpointID", false,
					      EndpointSet, 
					      EndpointGet));

    endpointEnv->addBinding("recipient", 
			   new PrimFieldValue("recipient", false,
					      EndpointSet, 
					      EndpointGet));

    endpointEnv->addBinding("pm", 
			   new PrimFieldValue("pm", false,
					      EndpointSet, 
					      EndpointGet));

    endpointEnv->addBinding("sq", 
			   new PrimFieldValue("sq", false,
					      EndpointSet, 
					      EndpointGet));

#if 0
    endpointEnv->addBinding("setProcess", 
			    new PrimMethodValue("setProcess", 1, 1,
					       EndpointMethod));

    endpointEnv->addBinding("setPayloadMatch", 
			    new PrimMethodValue("setPayloadMatch", 1, 1,
					       EndpointMethod));

    endpointEnv->addBinding("setSqueak", 
			    new PrimMethodValue("setSqueak", 1, 1,
					       EndpointMethod));
    endpointEnv->addBinding("setEndpointID", 
			    new PrimMethodValue("setEndpointID", 1, 1,
					       EndpointMethod));
    endpointEnv->addBinding("makeSendCap", 
			    new PrimMethodValue("makeSendCap", 0, 0,
					       EndpointMethod));
#endif
  }

  return endpointEnv;
}
