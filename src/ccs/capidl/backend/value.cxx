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

#include "SymTab.hxx"
#include "backend.hxx"

BigNum
compute_value(GCPtr<Symbol> s)
{
  switch(s->cls) {
  case sc_const:
    {
      return compute_value(s->value);
    }
  case sc_symRef:
    {
      return compute_value(s->value);
    }

  case sc_value:
    {
      switch(s->cls) {
      case lt_integer:
      case lt_unsigned:
	return s->v.bn;

      default:
	{
	  fprintf(stderr,
		  "Constant computation with bad value type \"%s\"\n",
		  s->name.c_str());
	  exit(1);
	}
      }
      break;
    }
    case sc_arithop:
      {
	BigNum left;
	BigNum right;
	char op;

	assert(s->children.size());

	op = s->v.bn.as_uint32();
	assert (op == '-' || s->children.size() > 1);

	left = compute_value(s->children[0]);
	if (op == '-' && s->children.size() == 1) {
	  left = left.neg();
	  return left;
	}
	  
	right = compute_value(s->children[1]);

	switch(op) {
	  case '-':
	    {
	      left = left - right;
	      return left;
	    }
	  case '+':
	    {
	      left = left + right;
	      return left;
	    }
	  case '*':
	    {
	      left = left * right;
	      return left;
	    }
	  case '/':
	    {
	      left = left / right;
	      return left;
	    }
	  default:
	    {
	      fprintf(stderr, "Unhandled arithmetic operator \"%s\"\n",
		      s->name.c_str());
	      exit(1);
	    }
	}
      }
  default:
    {
      fprintf(stderr, "Busted arithmetic\n");
      exit(1);
    }
  }

  return 0;
}
