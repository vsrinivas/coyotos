#ifndef PARSETYPE_HXX
#define PARSETYPE_HXX

/*
 * Copyright (C) 2006, The EROS Group, LLC.
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

#include <libsherpa/Path.hxx>
#include <libsherpa/LToken.hxx>
#include "AST.hxx"

typedef struct ParseType ParseType;
struct ParseType {
  sherpa::LToken     tok;	/* a literal string from the
				 * tokenizer. Used for strings,
				 * characters, numerical values. */

  GCPtr<AST> ast;

  ParseType()
  {
    ast = 0;
  }

  ~ParseType()
  {
    ast = 0;
  }
};

#endif /* PARSETYPE_HXX */
