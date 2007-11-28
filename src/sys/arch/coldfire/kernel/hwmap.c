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

/** @file
 * @brief Manipulation of the hardware mapping table.
 */

#include <stddef.h>
#include <kerninc/assert.h>
#include <kerninc/printf.h>
#include <kerninc/Mapping.h>
#include <kerninc/event.h>
#include <hal/transmap.h>

#include "hwmap.h"

Mapping KernMapping;

/* Following are placeholder implementations */
void
global_tlb_flush()
{
  local_tlb_flush();
}

void
global_tlb_flushva(kva_t va)
{
  local_tlb_flush();
}

#if 0
void
hwmap_enable_low_map()
{
  if (UsingPAE)
    KernPDPT.entry[0] = KernPDPT.entry[3];
  else
    KernPageDir[0] = KernPageDir[768];
}

void
hwmap_disable_low_map()
{
  if (UsingPAE)
    PTE_CLEAR(KernPDPT.entry[0]);
  else
    PTE_CLEAR(KernPageDir[0]);
}
#endif

/// @bug Need to grab locks here!
void
vm_switch_curcpu_to_map(Mapping *map)
{
  assert(map);

  LOG_EVENT(ety_MapSwitch, CUR_CPU->curMap, map, 0);

  if (map == CUR_CPU->curMap)
    return;

  CUR_CPU->curMap = map;

  transmap_advise_tlb_flush();
}
