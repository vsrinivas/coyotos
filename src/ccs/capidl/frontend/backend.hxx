#ifndef BACKEND_H
#define BACKEND_H
/*
 * Copyright (C) 2002, The EROS Group, LLC.
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

/* Helper function: */
extern BigNum compute_value(GCPtr<Symbol> s);

typedef void (*BackEndFn)(GCPtr<Symbol>);
typedef bool (*BackEndCheckFn)(GCPtr<Symbol>);
typedef void (*ScopeWalker)(GCPtr<Symbol> scope, BackEndFn outfn);

struct BackEnd {
  const char *   name;
  BackEndCheckFn typecheck;
  BackEndFn      xform;
  BackEndFn      fn;
  ScopeWalker    scopefn;
};

BackEnd *resolve_backend(const std::string& nm);

#endif /* BACKEND_H */
