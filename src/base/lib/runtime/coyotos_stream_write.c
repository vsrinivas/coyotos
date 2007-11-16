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

/* Canonical startup code for all domains */

#include <coyotos/captemp.h>
#include <idl/coyotos/IoStream.h>

bool
IDL_ENV_coyotos_IoStream_write(caploc_t _invCap,
			       coyotos_IoStream_chString s,
			       uint32_t *len,
			       IDL_Environment *_env)
{
  bool result;

  if (IDL_ENV_coyotos_IoStream_doWrite(_invCap, s, len, _env))
    return true;

  if (_env->errCode != RC_coyotos_IoStream_RequestWouldBlock)
    return false;

  /* We were asked to block. */
  _invCap = captemp_alloc();
    
  result = IDL_ENV_coyotos_IoStream_getWriteChannel(_invCap, _invCap, _env);
  if (!result) goto release;

  result = IDL_ENV_coyotos_IoStream_doWrite(_invCap, s, len, _env);

 release:
  captemp_release(_invCap);
  return result;
}
