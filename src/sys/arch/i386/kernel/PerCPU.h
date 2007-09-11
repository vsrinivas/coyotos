#ifndef I686_PERCPU_H
#define I686_PERCPU_H
/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

#include <coyotos/i686/target.h>
#include <kerninc/ccs.h>

#include "IA32/TSS.h"

/* This is the per-CPU data structure. It primarily includes the
   per-CPU stack but also some book-keeping information. Note that
   instances of this structure have alignment constraints that MUST be
   honored or the implementation of 'current()' will not work!!! */

/// Architecture-specific per-CPU data structure.
struct ArchPerCPU {
  ia32_TSS tss;  
};

#endif /* I686_PERCPU_H */
