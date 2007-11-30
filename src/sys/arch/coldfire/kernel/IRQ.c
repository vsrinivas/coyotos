/*
 * Copyright (C) 2007, The EROS Group, LLC
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
 *
 * @brief Data structures and initialization code for the Coldfire
 * interrupt handling mechanism.
 */
#include <hal/irq.h>
#include <kerninc/vector.h>
#include <kerninc/printf.h>

/** @brief Reverse map for finding vectors from IRQ numbers */
VectorInfo *IrqVector[NUM_IRQ];

/** @brief Handler function for user-mode interrupts. */
void
vh_BoundIRQ(VectorInfo *vec, Process *inProc, fixregs_t *saveArea)
{
  assert(!local_interrupts_enabled());

  vec->pending = 1;
  if (!sq_IsEmpty(&vec->stallQ)) {
    vec->next = CUR_CPU->wakeVectors;
    CUR_CPU->wakeVectors = vec;
  }
}

VectorInfo *
irq_MapInterrupt(irq_t irq)
{
  switch (IRQ_BUS(irq)) {
  case IBUS_GLOBAL:
    return IrqVector[irq];

  default:
    fatal("Unknown bus type for binding\n");
  }
}
