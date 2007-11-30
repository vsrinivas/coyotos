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
 * @brief Machine-independent code for interrupt vector management.
 */

#include <kerninc/vector.h>

void
irq_EnableVector(irq_t irq)
{
  VectorInfo *vector = irq_MapInterrupt(irq);

  assert(vector);

  VectorHoldInfo vhi = vector_grab(vector);

  vector->disableCount--;
  if (vector->disableCount == 0 && vector->pending)
    sq_WakeAll(&vector->stallQ, false);

  vector_release(vhi);
}

void
irq_DisableVector(irq_t irq)
{
  VectorInfo *vector = irq_MapInterrupt(irq);

  assert(vector);

  VectorHoldInfo vhi = vector_grab(vector);

  vector->disableCount++;

  vector_release(vhi);
}

bool
irq_isEnabled(irq_t irq)
{
  bool result = false;

  VectorInfo *vector = irq_MapInterrupt(irq);
  assert(vector);

  VectorHoldInfo vhi = vector_grab(vector);

  if (vector->disableCount == 0)
    result = true;

  vector_release(vhi);

  return result;
}
