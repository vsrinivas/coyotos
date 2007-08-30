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

#include <stdint.h>
#include <dirent.h>

#include "Value.hxx"
#include "Environment.hxx"
#include <assert.h>

using namespace sherpa;

template<class T>
GCPtr<Binding<T> >
Environment<T>::getLocalBinding(const std::string& nm) const
{
  for (size_t i = 0; i < bindings.size(); i++)
    if (bindings[i]->nm == nm)
      return bindings[i];

  return NULL;
}

template<class T>
GCPtr<Binding<T> > 
Environment<T>::getBinding(const std::string& nm) const
{
  GCPtr<Binding<T> > binding = getLocalBinding(nm);
  
  if (binding)
    return binding;

  if (parent)
    return parent->getBinding(nm);

  return NULL;
}

template<class T>
void
Environment<T>::removeBinding(const std::string& nm)
{
  for (size_t i = 0; i < bindings.size(); i++) {
    if (bindings[i]->nm == nm) {
      bindings.remove(i);
      return;
    }
  }

  if (parent)
    parent->removeBinding(nm);
}

template<class T>
void
Environment<T>::addBinding(const std::string& name, 
			    GCPtr<T> val, 
			    bool rebind)
{
  if (rebind) {
    GCPtr<Binding<T> > binding = getBinding(name);
    if (binding) {
      binding->val = val;
      binding->flags = 0;
      return;
    }
  }

  bindings.insert(0, new Binding<T>(name, val));
}

template<class T>
GCPtr<Environment<T> > 
Environment<T>::newScope()
{
  GCPtr<Environment<T> > nEnv = new Environment<T>(this);
  nEnv->parent = this;

  return nEnv;
}

template<class T>
Environment<T>::~Environment()
{
}

// EXPLICIT INSTANTIATIONS:

template GCPtr< Binding<Value> > 
Environment<Value>::getLocalBinding(const std::string& nm) const;

template GCPtr< Binding<Value> > 
Environment<Value>::getBinding(const std::string& nm) const;

template void
Environment<Value>::addBinding(const std::string& nm, GCPtr<Value> val,
			       bool rebind);

template void
Environment<Value>::removeBinding(const std::string& nm);

template GCPtr<Environment<Value> > 
Environment<Value>::newScope();

template
Environment<Value>::~Environment();
 
