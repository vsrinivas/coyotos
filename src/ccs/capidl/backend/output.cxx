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

#include <string.h>
#include <dirent.h>

#include <string>

/* Trick: introduce a sherpa declaration namespace with nothing in
   it so that the namespace is in scope for the USING declaration. */
namespace sherpa {};
using namespace sherpa;

#include "SymTab.hxx"
#include "backend.hxx"

extern void output_symdump(GCPtr<Symbol> );
extern void output_xmldoc(GCPtr<Symbol> );
extern void output_c_client_hdr(GCPtr<Symbol> );
extern void output_c_server_hdr(GCPtr<Symbol> );
//extern void output_new_c_hdr(GCPtr<Symbol> );
extern void output_c_hdr_depend(GCPtr<Symbol> , BackEndFn);
//extern void output_c_stubs(GCPtr<Symbol> );
//extern void output_c_stub_depend(GCPtr<Symbol> , BackEndFn);
extern void output_capidl(GCPtr<Symbol> );
extern void output_depend(GCPtr<Symbol> );
//extern void output_c_server(GCPtr<Symbol> , BackEndFn);
//extern void output_c_server_hdr(GCPtr<Symbol> , BackEndFn);
extern void output_html(GCPtr<Symbol> , BackEndFn);
extern void output_c_template(GCPtr<Symbol> , BackEndFn);
extern void output_server_dependent_headers(GCPtr<Symbol> , BackEndFn);

extern void rewrite_for_c(GCPtr<Symbol> );
extern bool c_typecheck(GCPtr<Symbol> );

BackEnd back_ends[] = {
  { "raw",       0, 	      0,             output_symdump,       0  },
  { "xml",       0,           0,             output_xmldoc,        0  },
  { "c-server-header",  c_typecheck, 0,	     output_c_server_hdr,
    output_server_dependent_headers  },
  { "c-client-header",  c_typecheck, 0,	     output_c_client_hdr,  0  },
  { "c-template",       c_typecheck, 0,	     0,  output_c_template },
  // Following is purely temporary:
  { "new-c-header",  c_typecheck, 0,         output_c_client_hdr,  0  },
  { "c-header-depend", 
                 0,           0,             0,  output_c_hdr_depend  },
  //  { "c-stub",    c_typecheck, rewrite_for_c, output_c_stubs,       0  },
  //  { "c-stub-depend", 
  //                 0,           0,             0,  output_c_stub_depend  },
//{ "c-server",  c_typecheck, rewrite_for_c, 0,       output_c_server  },
//  { "c-server-header",
//                 c_typecheck, rewrite_for_c, 0,       output_c_server_hdr },
  { "capidl",    0,           0,             output_capidl,        0  },
  { "depend",    0,           0,             output_depend,        0  },
  { "html",      0,           0,             0,          output_html  },
  //  { "new-c-header",
  //                 c_typecheck, 0,             output_new_c_hdr,     0  },
};

BackEnd *
resolve_backend(const std::string& nm)
{
  size_t i;

#if 0
  if (nm.empty())
    return &back_ends[0];
#else
  if (nm.empty())
    return 0;
#endif

  for (i = 0; i < sizeof(back_ends)/ sizeof(BackEnd); i++) {
    if ( nm == back_ends[i].name )
      return &back_ends[i];
  }

  return 0;
}

