#ifndef ENVIRONMENT_HXX
#define ENVIRONMENT_HXX
/*
 * Copyright (C) 2005, The EROS Group, LLC.
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

#include <libsherpa/CVector.hxx>
#include <iostream>

class UocInfo;

// Type of (sub) environment, if any.
// Universal, In module scope, or in record scope
//enum envType {universal, mod, rec, none}; 
//enum BindType {bind_type, bind_value};


#define BF_EXPORTED   0x1u  /* Binding is exported */
#define BF_CONSTANT   0x2u  /* Binding is immutable */
#define BF_PURE       0x4u  /* Binding is pure */

template <class T>
struct Binding : public sherpa::Countable {
  std::string nm;
  sherpa::GCPtr<T> val;
  unsigned flags;

  Binding(const std::string& _nm, sherpa::GCPtr<T> _val)
  {
    nm = _nm;
    val = _val;
    flags = 0;
  }
};

template <class T>
struct Environment : public sherpa::Countable {
  sherpa::GCPtr<Environment<T> > parent; // in the chain of environments

  sherpa::CVector<sherpa::GCPtr<Binding<T> > > bindings;

  sherpa::GCPtr<Binding<T> > 
  getBinding(const std::string& nm) const;

  sherpa::GCPtr<Binding<T> > 
  getLocalBinding(const std::string& nm) const;

  Environment(sherpa::GCPtr<Environment> _parent = 0)
  {
    parent = _parent;
  }

  ~Environment();

  void
  addBinding(const std::string& name, sherpa::GCPtr<T> val, 
	     bool rebind = false);

  void
  removeBinding(const std::string& name);

  inline sherpa::GCPtr<T>
  getValue(const std::string& nm) const
  {
    const sherpa::GCPtr<Binding<T> > binding = getBinding(nm);
    return (binding ? binding->val : NULL);
  }

  inline unsigned
  getFlags(const std::string& nm)
  {
    const sherpa::GCPtr<Binding<T> > binding = getBinding(nm);
    return (binding ? binding->flags : 0);
  }

  inline void
  setFlags(const std::string& nm, unsigned long flags)
  {
    sherpa::GCPtr<Binding<T> > binding = getBinding(nm);
    if (binding) binding->flags |= flags;
  }

  void
  addConstant(const std::string& name, sherpa::GCPtr<T> val)
  {
    addBinding(name, val);
    setFlags(name, BF_CONSTANT);
  }

  void
  addPureConstant(const std::string& name, sherpa::GCPtr<T> val)
  {
    addBinding(name, val);
    setFlags(name, BF_CONSTANT|BF_PURE);
  }

  sherpa::GCPtr<Environment<T> >newScope();
};
#endif /* ENVIRONMENT_HXX */
