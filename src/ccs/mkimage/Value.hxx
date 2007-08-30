#ifndef VALUE_HXX
#define VALUE_HXX

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

#include <inttypes.h>
#include <assert.h>

#include <string>
#include <libsherpa/BigNum.hxx>
#include <libsherpa/CVector.hxx>
#include <libsherpa/UExcept.hxx>

#include <obstore/capability.h>
#include <libcoyimage/CoyImage.hxx>
#include "Environment.hxx"
#include "Interp.hxx"

struct AST;

/// @brief Common base class for all values.
///
/// The value notion for mkimage used to be simple. That, of course,
/// required repair.
///
/// The problem that needs to be solved is that some of our ``values''
/// are actually stored in fields of foreign structures. This means
/// that we need a proxy-value notion. Further, the introduction of
/// foreign fields means that we need something like a closure,
/// because the proxy value object needs to retain a pointer (or
/// equivalent) to the foreign object.
///
/// In handling foreign objects, we want to be able to create an
/// ``environment'' for each object type that describes its methods
/// and fields. Rather than generate the environment dynamically when
/// we do "object.field", we would like one statically allocated
/// environment per object type. The problem is that if the
/// environment is statically allocated it's field descriptors cannot
/// already contain the requisite object pointers.
///
/// The solution is to introduce a notion of "open" and "closed"
/// values. An "open" value is one that requires an object pointer but
/// doesn't yet have one. A "closed" value is one that either @em has
/// an object pointer or doesn't require one (e.g. scalars). Within
/// the interpreter, we will check whenever we process a.b. If the
/// value of a.b is an ``open'' value we will make a copy of it, close
/// the copy by inserting the value of a, and return that instead of
/// the open value.
///
/// We then introduce the idea that @em every value (open or closed)
/// has two methods get()->ClosedValue, and set(ClosedValue)->void.
///
/// For scalar values, the get() method simply returns 'this', the
/// set() method raises an exception.
///
/// For foreign fields, the get() method returns the value stored in
/// that field <em>expressed as a mkimage scalar value</em>. The set()
/// method updates the foreign field. Note it is important that the
/// foreign value type @em cache the rvalue, because the call to get()
/// may be deferred and may conceivably cross a set() boundary,
/// resulting in an incorrect answer!
///
/// The reason that we need the apply() method is that methods of
/// foreign objects need to be closed over their object pointer.
///
/// Now the @b only reason that this works is that we @b never allow
/// an OpenValue to escape the dot operation (the "a.b") that looked
/// it up. For example, in the expression "cap.l2v", the internal
/// binding of "lwv" is an OpenValue that accesses a foreign field,
/// but if we write:
///
///           def x = cap.l2v
///
/// the variable 'x' receives a closed value that is a *copy* of
/// the field value. The only place that an OpenValue can be
/// introduced is in the built-in procedure layer. This absolves us of
/// any concern that get() might need to be recursive.

struct Value : public sherpa::Countable {
  enum ValKind {
    vk_unit,
    vk_bool,
    vk_char,
    vk_int,
    vk_float,
    vk_string,
    vk_cap,			// capability
    vk_function,		// Top-level define, no closure, AST value
    vk_primfn,			// native-implemented functions
    vk_primMethod,		// native-implemented methods
    vk_primField,		// native-implemented fields
    vk_env,			// named scope -- value is an environment
    vk_boundValue,

  };

  const ValKind kind;
  bool isOpen;

  Value(ValKind k)
    :kind(k)
  {
    isOpen = false;
  }

  virtual sherpa::GCPtr<Environment<Value> > getEnvironment()
  {
    return 0;
  }

  virtual sherpa::GCPtr<Value> get()
  {
    // scalars have trivial getters:
    return this;
  }

  virtual void set(sherpa::GCPtr<Value>)
  {
    // scalars do not implement set()
    throw sherpa::excpt::NoAccess;
  }

  // Need something virtual so that RTTI works:
  virtual ~Value();
};

extern Value TheUnitValue;

struct BoolValue : public Value {
  bool   b;			/* boolean Values */

  BoolValue(const std::string& _value)
    :Value(vk_bool)
  {
    if (_value == "true" || _value == "#t")
      b = true;
    else
      b = false;
  }

  BoolValue(bool _value)
    :Value(vk_bool)
  {
    b = _value;
  }
} ;

struct CharValue : public Value {
private:
  static uint32_t DecodeRawCharacter(const char *s, const char **next);
  static uint32_t DecodeCharacter(const std::string&);

public:
  unsigned long c;		/* utf32 code points */

  CharValue(const std::string& _value)
    :Value(vk_char)
  {
    c = DecodeCharacter(_value);
  }

  CharValue(uint32_t _value)
    :Value(vk_char)
  {
    c = _value;
  }
};

struct IntValue : public Value {
  sherpa::BigNum  bn;		// large precision integers

  unsigned long base;		// base as originally expressed

  IntValue(const std::string& _value);
  IntValue(const sherpa::BigNum& _value)
    :Value(vk_int)
  {
    bn = _value;
    base = 0;
  }
};

struct FloatValue : public Value {
  double d;

  unsigned long base;		// base as originally expressed

  FloatValue(const std::string& _value);
  FloatValue(double _value)
    :Value(vk_float)
  {
    d = _value;
    base = 0;
  }
};

struct StringValue : public Value {
  // Helper for AST
  static uint32_t DecodeStringCharacter(const char *s, const char **next);

public:
  std::string s;  /* String Literals          */

  StringValue(const std::string& _value)
    :Value(vk_string)
  {
    s = _value;
  }
};

#if 0
struct RefValue : public Value {
  sherpa::GCPtr<Value> v;			/* Indirect values          */

  RefValue(sherpa::GCPtr<Value> _value)
    :Value(vk_ref)
  {
    v = _value;
  }
};
#endif

struct FnValue : public Value {
  std::string nm;
  sherpa::GCPtr<Environment<Value> > closedEnv;
  sherpa::GCPtr<AST> args;
  sherpa::GCPtr<AST> body;

  FnValue(std::string id, 
	  const sherpa::GCPtr<Environment<Value> > _closedEnv,
	  const sherpa::GCPtr<AST>& _args, 
	  const sherpa::GCPtr<AST>& _body);
};

struct PrimFnValue : public Value {
  std::string nm;		// purely for diagnostics
  const size_t minArgs;
  const size_t maxArgs;		/* 0 == any number */
  sherpa::GCPtr<Value> (*fn)(PrimFnValue&,
			     InterpState&,
			     sherpa::CVector<sherpa::GCPtr<Value> >&); // function to call

  PrimFnValue(std::string id, size_t _nArgs,
	      sherpa::GCPtr<Value> 
	      (*_fn)(PrimFnValue&,
		     InterpState&,
		     sherpa::CVector<sherpa::GCPtr<Value> >&))
    :Value(vk_primfn),
     minArgs(_nArgs), maxArgs(_nArgs)
  {
    nm = id;			// purely for diagnostics
    fn = _fn;
  }

  PrimFnValue(std::string id, size_t _minArgs, size_t _maxArgs,
	      sherpa::GCPtr<Value> 
	      (*_fn)(PrimFnValue&,
		     InterpState&,
		     sherpa::CVector<sherpa::GCPtr<Value> >&))
    :Value(vk_primfn),
     minArgs(_minArgs), maxArgs(_maxArgs)
  {
    nm = id;			// purely for diagnostics
    fn = _fn;
  }
};

struct OpenValue : public Value {
  sherpa::GCPtr<Value> thisValue;

  OpenValue(ValKind k)
    :Value(k)
  {
    isOpen = true;
  }

  virtual sherpa::GCPtr<OpenValue> dup() = 0;

  virtual sherpa::GCPtr<Value> closeWith(sherpa::GCPtr<Value> value)
  {
    sherpa::GCPtr<OpenValue> vp = dup();
    vp->thisValue = value;
    vp->isOpen = false;

    return vp;
  }
};

struct PrimMethodValue : public OpenValue {
  std::string nm;		// purely for diagnostics
  const size_t minArgs;
  const size_t maxArgs;		/* 0 == any number */
  sherpa::GCPtr<Value> (*fn)(PrimMethodValue&,
			     InterpState&,
			     sherpa::CVector<sherpa::GCPtr<Value> >&); // function to call

  PrimMethodValue(std::string id, size_t _nArgs,
		  sherpa::GCPtr<Value> 
		  (*_fn)(PrimMethodValue&,
			 InterpState&,
			 sherpa::CVector<sherpa::GCPtr<Value> >&))
    :OpenValue(vk_primMethod),
     minArgs(_nArgs), maxArgs(_nArgs)
  {
    nm = id;			// purely for diagnostics
    fn = _fn;
  }

  PrimMethodValue(std::string id, size_t _minArgs, size_t _maxArgs,
		  sherpa::GCPtr<Value> 
		  (*_fn)(PrimMethodValue&,
			 InterpState&,
			 sherpa::CVector<sherpa::GCPtr<Value> >&))
    :OpenValue(vk_primMethod),
     minArgs(_minArgs), maxArgs(_maxArgs)
  {
    nm = id;			// purely for diagnostics
    fn = _fn;
  }

  PrimMethodValue(const PrimMethodValue& pmv)
    : OpenValue(pmv.kind),
     minArgs(pmv.minArgs), maxArgs(pmv.maxArgs)
  {
    nm = pmv.nm;
    fn = pmv.fn;
  }

  sherpa::GCPtr<OpenValue> dup();

  sherpa::GCPtr<Value> get()
  {
    return this;
  }
};

/// @brief A (possibly settable) field of an externally defined
/// (foreign) object.
struct PrimFieldValue : public OpenValue {
  bool isVector;
  size_t ndx;
  bool isConstant;
  std::string ident;
  void (*doSet)(PrimFieldValue&,
		sherpa::GCPtr<Value> &);

  sherpa::GCPtr<Value> (*doGet)(PrimFieldValue&);

  sherpa::GCPtr<Value> rValue;

  void cacheTheValue()
  {
    rValue = doGet(*this);
  }

  PrimFieldValue(std::string _ident, bool _isConstant,
		 void (*_doSet)(PrimFieldValue&,
				sherpa::GCPtr<Value> &),
		 sherpa::GCPtr<Value> (*_doGet)(PrimFieldValue&),
		 bool _isVector = false
		 )
    :OpenValue(vk_primField)
  {
    isConstant = _isConstant;
    ident = _ident;
    doSet = _doSet;
    doGet = _doGet;
    isVector = _isVector;
    ndx = ~0u;
  }

  PrimFieldValue(const PrimFieldValue& pmv)
    : OpenValue(pmv.kind)
  {
    isConstant = pmv.isConstant;
    ident = pmv.ident;
    doSet = pmv.doSet;
    doGet = pmv.doGet;
    isVector = pmv.isVector;
    ndx = ~0u;
  }

  sherpa::GCPtr<OpenValue> dup();

  sherpa::GCPtr<Value> get()
  {
    return rValue;
  }

  virtual sherpa::GCPtr<Value> closeWith(sherpa::GCPtr<Value> value)
  {
    sherpa::GCPtr<Value> v = OpenValue::closeWith(value);
    if (!isVector)
      v.upcast<PrimFieldValue>()->cacheTheValue();
    
    return v;
  }

  void set(sherpa::GCPtr<Value> v)
  {
    doSet(*this, v);
  }
};

class CapValue : public Value {
  sherpa::GCPtr<Environment<Value> > getWrapperEnvironment();
  sherpa::GCPtr<Environment<Value> > getEndpointEnvironment();
  sherpa::GCPtr<Environment<Value> > getGptEnvironment();
  sherpa::GCPtr<Environment<Value> > getProcessEnvironment();
public:
  capability              cap;
  sherpa::GCPtr<CoyImage> ci;

  sherpa::GCPtr<Environment<Value> > getEnvironment();

  // This will throw if the capability is not a CapPage capability.
  sherpa::GCPtr<PrimFieldValue> derefAsVector(size_t ndx);

  CapValue(const sherpa::GCPtr<CoyImage>& _ci, const capability& _c)
    :Value(vk_cap)
  {
    cap = _c;
    ci = _ci;
  }
};

/// @brief A first-class environment
struct EnvValue : public Value {
  sherpa::GCPtr<Environment<Value> > env;

  sherpa::GCPtr<Environment<Value> > getEnvironment()
  {
    return env;
  }

  EnvValue(const sherpa::GCPtr<Environment<Value> >& _env)
    :Value(vk_env)
  {
    env = _env;
  }
};

/// @brief A ``wrapped'' value that encapsulates the binding of a
/// value within an environment.
///
/// This exists to support l-values. See the lengthy comment on the
/// Value class.
struct BoundValue : public Value {
  sherpa::GCPtr<Binding<Value> > binding;
  sherpa::GCPtr<Value > rvalue;

  BoundValue(const sherpa::GCPtr<Binding<Value> >& _binding)
    :Value(vk_boundValue)
  {
    binding = _binding;
    rvalue = binding->val;	// must cache eagerly
  }

  sherpa::GCPtr<Value> get()
  {
    return rvalue;
  }

  void set(sherpa::GCPtr<Value> v);
};

#endif /* VALUE_HXX */
