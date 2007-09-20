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
 * @brief LAPIC interrupt controller support.
 */

#include <kerninc/printf.h>
#include "lapic.h"
#include "IRQ.h"
#include "PIC.h"

static void
lapic_setup(VectorInfo *vi)
{
  uint32_t val;

  switch(vi->irq) {
  case irq_LAPIC_Spurious:
    {
      val = lapic_read_register(LAPIC_SVR);
      val &= ~LAPIC_SVR_VECTOR_MASK;
      val = val | (vi - &VectorMap[0]);
      lapic_write_register(LAPIC_SVR, val);
      break;
    }
  case irq_LAPIC_Timer:
    {
      val = lapic_read_register(LAPIC_LVT_Timer);
      val &= ~LAPIC_LVT_VECTOR_MASK;
      val = val | (vi - &VectorMap[0]);
      lapic_write_register(LAPIC_LVT_Timer, val);
      break;
    }
  case irq_LAPIC_IPI:
    break;
  default:
    fatal("Cannot enable lapic IRQ 0x%x\n", vi->irq);
  }


  /* Already set up at initialization time. */
}

static void
lapic_enable(VectorInfo *vi)
{
  switch(vi->irq) {
  case irq_LAPIC_Spurious:
    {
      /* This is per-cpu. It should not be done here. */
      uint32_t val = lapic_read_register(LAPIC_SVR);
      lapic_write_register(LAPIC_SVR, val | LAPIC_SVR_ENABLED);
      break;
    }
  case irq_LAPIC_Timer:
    {
      uint32_t val = lapic_read_register(LAPIC_LVT_Timer);
      lapic_write_register(LAPIC_SVR, val & ~LAPIC_LVT_MASKED);
      break;
    }
  default:
    fatal("Cannot enable lapic IRQ 0x%x\n", vi->irq);
  }
}

static void
lapic_disable(VectorInfo *vi)
{
  switch(vi->irq) {
  case irq_LAPIC_Spurious:
    {
      /* This is per-cpu. It should not be done here. */
      uint32_t val = lapic_read_register(LAPIC_SVR);
      lapic_write_register(LAPIC_SVR, val & ~LAPIC_SVR_ENABLED);
      break;
    }
  case irq_LAPIC_Timer:
    {
      uint32_t val = lapic_read_register(LAPIC_LVT_Timer);
      lapic_write_register(LAPIC_SVR, val | LAPIC_LVT_MASKED);
      break;
    }
  default:
    fatal("Cannot enable lapic IRQ 0x%x\n", vi->irq);
  }
}

static bool
lapic_isPending(VectorInfo *vi)
{
  return true;
}

static void
lapic_acknowledge(VectorInfo *vi)
{
  lapic_eoi();
}

static IrqController lapic = {
  0,
  16,
  0,
  lapic_setup,
  lapic_enable,
  lapic_disable,
  lapic_isPending,
  lapic_acknowledge,
  pic_no_op,
};

void
lapic_spurious_interrupt()
{
  fatal("Spurious lapic interrupt!\n");
}

/** @brief Initialize the LAPIC interrupt controller.
 */
void
lapic_init()
{
  assert(cpu_ncpu == 1);

  printf("LAPIC TIMER: 0x%08x\n", lapic_read_register(LAPIC_LVT_Timer));
  printf("LAPIC LINT0: 0x%08x\n", lapic_read_register(LAPIC_LVT_LINT0));
  printf("LAPIC LINT1: 0x%08x\n", lapic_read_register(LAPIC_LVT_LINT1));
  printf("LAPIC ERROR: 0x%08x\n", lapic_read_register(LAPIC_LVT_ERROR));
  printf("LAPIC PCINT: 0x%08x\n", lapic_read_register(LAPIC_LVT_PerfCntr));

  {
    VectorInfo *vector = irq_MapInterrupt(irq_LAPIC_Timer);
    vector->type = vt_Interrupt;
    vector->mode = VEC_MODE_EDGE;
    vector->level = VEC_LEVEL_ACTHIGH;
    vector->irq = irq_LAPIC_Timer;
    vector->enabled = 0;
    vector->ctrlr = &lapic;
  }

  {
    VectorInfo *vector = irq_MapInterrupt(irq_LAPIC_IPI);
    vector->type = vt_Interrupt;
    vector->mode = VEC_MODE_EDGE;
    vector->level = VEC_LEVEL_ACTLOW; /* ?? */
    vector->irq = irq_LAPIC_IPI;
    vector->enabled = 0;
    vector->ctrlr = &lapic;
  }

  {
    VectorInfo *vector = irq_MapInterrupt(irq_LAPIC_Spurious);
    vector->type = vt_Interrupt;
    vector->mode = VEC_MODE_EDGE;
    vector->level = VEC_LEVEL_ACTHIGH; /* ??? */
    vector->irq = irq_LAPIC_IPI;
    vector->enabled = 0;
    vector->ctrlr = &lapic;
  }

  irq_Bind(irq_LAPIC_Spurious, VEC_MODE_EDGE, VEC_LEVEL_ACTHIGH, 
	   lapic_spurious_interrupt);
}
