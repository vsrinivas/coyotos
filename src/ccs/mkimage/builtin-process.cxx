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

static GCPtr<Environment<Value> > processEnv = 0;

// Set field values:
static void 
ProcessSet(PrimFieldValue& pfv,
	   GCPtr<Value> & val)
{
  GCPtr<CapValue> thisv =  pfv.thisValue.upcast<CapValue>();
  GCPtr<CoyImage> ci = thisv->ci;

  GCPtr<CiProcess> ob = ci->GetProcess(thisv->cap);

  if (pfv.ident == "runState") {
    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    uint32_t rs = ob->v.runState = val.upcast<IntValue>()->bn.as_uint32();

    switch(rs) {
    case PRS_RUNNING:
    case PRS_RECEIVING:
    case PRS_FAULTED:
      ob->v.runState = val.upcast<IntValue>()->bn.as_uint32();
      break;
    default:
      throw excpt::BoundsError;
    }
    return;
  }

  if (pfv.ident == "flags") {
    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.flags = val.upcast<IntValue>()->bn.as_uint32();
    return;
  }

  if (pfv.ident == "notices") {
    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.notices = val.upcast<IntValue>()->bn.as_uint32();
    return;
  }

  if (pfv.ident == "faultCode") {
    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.faultCode = val.upcast<IntValue>()->bn.as_uint32();
    return;
  }

  if (pfv.ident == "faultInfo") {
    if (val->kind != Value::vk_int)
      throw excpt::BadValue;

    ob->v.faultInfo = val.upcast<IntValue>()->bn.as_uint64();
    return;
  }

  if (pfv.ident == "schedule") {
    if (val->kind != Value::vk_cap)
      throw excpt::BadValue;

    ob->v.schedule = val.upcast<CapValue>()->cap;
    return;
  }

  if (pfv.ident == "addrSpace") {
    if (val->kind != Value::vk_cap)
      throw excpt::BadValue;

    ob->v.addrSpace = val.upcast<CapValue>()->cap;
    return;
  }

  if (pfv.ident == "brand") {
    if (val->kind != Value::vk_cap)
      throw excpt::BadValue;

    ob->v.brand = val.upcast<CapValue>()->cap;
    return;
  }

  if (pfv.ident == "cohort") {
    if (val->kind != Value::vk_cap)
      throw excpt::BadValue;

    ob->v.cohort = val.upcast<CapValue>()->cap;
    return;
  }

  if (pfv.ident == "ioSpace") {
    if (val->kind != Value::vk_cap)
      throw excpt::BadValue;

    ob->v.ioSpace = val.upcast<CapValue>()->cap;
    return;
  }

  if (pfv.ident == "handler") {
    if (val->kind != Value::vk_cap)
      throw excpt::BadValue;

    ob->v.handler = val.upcast<CapValue>()->cap;
    return;
  }

  if (pfv.ident == "capReg") {
    if (pfv.ndx == ~0u || pfv.ndx >= NUM_CAP_REGS || pfv.ndx == 0) {
      // Cannot set() an un-dereferenced vector or exceed the bound.
      // capReg 0 must remain null.
      throw excpt::BadValue;
    }

    ob->v.capReg[pfv.ndx] = val.upcast<CapValue>()->cap;
    return;
  }

  THROW(excpt::BadValue, "Unknown field name");
}

// Get field values:
static GCPtr<Value> 
ProcessGet(PrimFieldValue& pfv)
{
  // Only field is "cap", so no need to bother with dispatching:
  
  GCPtr<CapValue> thisv =  pfv.thisValue.upcast<CapValue>();
  GCPtr<CoyImage> ci = thisv->ci;

  GCPtr<CiProcess> ob = ci->GetProcess(thisv->cap);

  if (pfv.ident == "flags")
    return new IntValue(ob->v.flags);
  if (pfv.ident == "runState")
    return new IntValue(ob->v.runState);
  if (pfv.ident == "notices")
    return new IntValue(ob->v.notices);
  if (pfv.ident == "faultCode")
    return new IntValue(ob->v.faultCode);
  if (pfv.ident == "faultInfo")
    return new IntValue(ob->v.faultInfo);

  if (pfv.ident == "schedule")
    return new CapValue(ci, ob->v.schedule);
  if (pfv.ident == "addrSpace")
    return new CapValue(ci, ob->v.addrSpace);
  if (pfv.ident == "brand")
    return new CapValue(ci, ob->v.brand);
  if (pfv.ident == "cohort")
    return new CapValue(ci, ob->v.cohort);
  if (pfv.ident == "ioSpace")
    return new CapValue(ci, ob->v.ioSpace);
  if (pfv.ident == "handler")
    return new CapValue(ci, ob->v.handler);

  if (pfv.ident == "capReg") {
    if (pfv.ndx == ~0u || pfv.ndx >= NUM_CAP_REGS) {
      // Cannot get() an un-dereferenced vector or exceed the bound
      throw excpt::BadValue;
    }

    return new CapValue(ci, ob->v.capReg[pfv.ndx]);
  }

  THROW(excpt::BadValue, "Unknown field name");
}

GCPtr<Environment<Value> > 
CapValue::getProcessEnvironment()
{
  if (!processEnv) {
    processEnv = new Environment<Value>;

    processEnv->addBinding("runState", 
			   new PrimFieldValue("runState", false,
					      ProcessSet, 
					      ProcessGet));

    processEnv->addBinding("flags", 
			   new PrimFieldValue("flags", false,
					      ProcessSet, 
					      ProcessGet));

    processEnv->addBinding("faultCode", 
			   new PrimFieldValue("faultCode", false,
					      ProcessSet, 
					      ProcessGet));

    processEnv->addBinding("faultInfo", 
			   new PrimFieldValue("faultInfo", false,
					      ProcessSet, 
					      ProcessGet));
    processEnv->addBinding("schedule", 
			   new PrimFieldValue("schedule", false,
					      ProcessSet, 
					      ProcessGet));
    processEnv->addBinding("addrSpace", 
			   new PrimFieldValue("addrSpace", false,
					      ProcessSet, 
					      ProcessGet));
    processEnv->addBinding("brand", 
			   new PrimFieldValue("brand", false,
					      ProcessSet, 
					      ProcessGet));
    processEnv->addBinding("cohort", 
			   new PrimFieldValue("cohort", false,
					      ProcessSet, 
					      ProcessGet));
    processEnv->addBinding("ioSpace", 
			   new PrimFieldValue("ioSpace", false,
					      ProcessSet, 
					      ProcessGet));
    processEnv->addBinding("handler", 
			   new PrimFieldValue("handler", false,
					      ProcessSet, 
					      ProcessGet));
    processEnv->addBinding("upcb", 
			   new PrimFieldValue("upcb", false,
					      ProcessSet, 
					      ProcessGet));
    processEnv->addBinding("capReg", 
			   new PrimFieldValue("capReg", false,
					      ProcessSet, 
					      ProcessGet,
					      true));

  }

  return processEnv;
}
