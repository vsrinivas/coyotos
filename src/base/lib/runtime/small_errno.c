/*
 * Copyright (C) 2007, Jonathan S. Shapiro.
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

#include <sys/errno.h>

/** @brief Definition of errno for newlib/coyotos
 *
 * This function is (regrettably) replicated in the small-space and
 * large-space libraries. Unfortunately, sbrk may turn out to be the
 * only thing in the binary that references errno, with the result
 * that it cannot be reliably pulled from -lc_coyotos. 
 *
 * Alternative solutions would be to have model-specific versions of
 * libc-coyotos.a, or to rotate things around such that libc_coyotos.a
 * came AFTER libsmall-space.a in the default link order. Not clear
 * what is best there. Probably multiple libc_coyotos.a models.
 */

int *__errno(void)
{
  static int the_real_errno;

  return &the_real_errno;
}
