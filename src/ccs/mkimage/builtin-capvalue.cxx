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

// Set field values:
static void 
CapPageSet(PrimFieldValue& pfv,
	   GCPtr<Value> & val)
{
  GCPtr<CapValue> thisv =  pfv.thisValue.upcast<CapValue>();
  GCPtr<CoyImage> ci = thisv->ci;

  GCPtr<CiCapPage> ob = ci->GetCapPage(thisv->cap);

  size_t nSlots = ob->pgSize/sizeof(capability);

  if (pfv.ndx >= nSlots) {
    // Cannot set() an un-dereferenced vector or exceed the bound
    throw excpt::BoundsError;
  }

  try {
    ob->data[pfv.ndx] = val.upcast<CapValue>()->cap;
  }
  catch(...) {
    throw excpt::BadValue;
  }

  return;
}

// Get field values:
static GCPtr<Value> 
CapPageGet(PrimFieldValue& pfv)
{
  // Only field is "cap", so no need to bother with dispatching:
  
  GCPtr<CapValue> thisv =  pfv.thisValue.upcast<CapValue>();
  GCPtr<CoyImage> ci = thisv->ci;

  GCPtr<CiCapPage> ob = ci->GetCapPage(thisv->cap);

  size_t nSlots = ob->pgSize/sizeof(capability);

  if (pfv.ndx >= nSlots) {
    // Cannot set() an un-dereferenced vector or exceed the bound
    throw excpt::BadValue;
  }

  return new CapValue(ci, ob->data[pfv.ndx]);
}


// Set field values:
static void 
PageSet(PrimFieldValue& pfv,
	GCPtr<Value> & val)
{
  GCPtr<CapValue> thisv =  pfv.thisValue.upcast<CapValue>();
  GCPtr<CoyImage> ci = thisv->ci;

  GCPtr<CiPage> ob = ci->GetPage(thisv->cap);

  size_t nSlots = ob->pgSize;

  if (pfv.ndx >= nSlots) {
    // Cannot set() an un-dereferenced vector or exceed the bound
    throw excpt::BadValue;
  }

  GCPtr<IntValue> iv = val.upcast<IntValue>();
  if (iv->bn > 255)
    // Exceeds byte value bound.
    throw excpt::BadValue;

  ob->data[pfv.ndx] = iv->bn.as_uint32();
  return;
}

// Get field values:
static GCPtr<Value> 
PageGet(PrimFieldValue& pfv)
{
  // Only field is "cap", so no need to bother with dispatching:
  
  GCPtr<CapValue> thisv =  pfv.thisValue.upcast<CapValue>();
  GCPtr<CoyImage> ci = thisv->ci;

  GCPtr<CiPage> ob = ci->GetPage(thisv->cap);

  size_t nSlots = ob->pgSize;

  if (pfv.ndx >= nSlots) {
    // Cannot set() an un-dereferenced vector or exceed the bound
    throw excpt::BadValue;
  }

  return new IntValue(ob->data[pfv.ndx]);
}


sherpa::GCPtr<PrimFieldValue> 
CapValue::derefAsVector(size_t ndx)
{
  if (cap.type == ct_CapPage) {
    sherpa::GCPtr<PrimFieldValue> pfv =
      new PrimFieldValue("<capPage>", false,
			 CapPageSet, 
			 CapPageGet,
			 true);
      
    pfv = pfv->closeWith(this).upcast<PrimFieldValue>();
    pfv->ndx = ndx;
    pfv->cacheTheValue();
      
    return pfv;
  }
  else if (cap.type == ct_Page) {
    sherpa::GCPtr<PrimFieldValue> pfv =
      new PrimFieldValue("<page>", false,
			 PageSet, 
			 PageGet,
			 true);
      
    pfv = pfv->closeWith(this).upcast<PrimFieldValue>();
    pfv->ndx = ndx;
    pfv->cacheTheValue();
      
    return pfv;
  }
  else
    throw excpt::BadValue;
}

