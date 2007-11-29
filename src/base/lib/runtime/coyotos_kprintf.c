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

#include <stdarg.h>
#include <coyotos/kprintf.h>

bool
IDL_ENV_kprintf(IDL_Environment *_env, caploc_t kernlog,
		const char *fmt, ...)
{
  va_list	listp;
  va_start(listp, fmt);

  bool result = IDL_ENV_kvprintf(_env, kernlog, fmt, listp);

  va_end(listp);

  return result;
}

/** @brief log to a KernLog capability. */
bool
kprintf(caploc_t kernlog, const char *fmt, ...)
{
  va_list	listp;
  va_start(listp, fmt);

  bool result = IDL_ENV_kvprintf(__IDL_Env, kernlog, fmt, listp);

  va_end(listp);

  return result;
}
