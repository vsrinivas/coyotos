#ifndef INTERP_HXX
#define INTERP_HXX

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

#include <libcoyimage/CoyImage.hxx>
#include "Environment.hxx"
struct AST;
struct Value;

struct InterpState {
  std::ostream &errStream;
  sherpa::GCPtr<AST> curAST;
  sherpa::GCPtr<Environment<Value> > env;
  sherpa::GCPtr<CoyImage> ci;

  /// @brief Whether we are interpreting in pure mode.
  ///
  /// In constant mode, we will only perform enough interpretation to
  /// establish constant definitions. The effect of this is to limit
  /// the expression interpreter to prohibit fncall on most procedures
  /// and to require all identifier resolutions to yield constant
  /// identifiers. This mode was introduced in order to allow us to
  /// evaluate expressions on the right hand side of enumerations.
  ///
  /// As special cases, fncall on arithmetic procedures and their ilk
  /// remain enabled.
  ///
  /// InterpState initializes to constMode = false;
  bool pureMode;

  InterpState(std::ostream & _errStream, 
	      sherpa::GCPtr<CoyImage> _ci,
	      sherpa::GCPtr<AST> _curAST,
	      sherpa::GCPtr<Environment<Value> > _env
	      );

  InterpState(const InterpState& is);
};

#endif /* INTERP_HXX */
