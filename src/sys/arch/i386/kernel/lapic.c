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
#include "8259.h"

static inline uint32_t 
lapic_irq_register(irq_t irq)
{
  switch(irq) {
  case irq_LAPIC_SVR:
    return LAPIC_SVR;
  case irq_LAPIC_Timer:
    return LAPIC_LVT_Timer;
#if 0
  case irq_LAPIC_ERROR:
    return LAPIC_LVT_ERROR;
  case irq_LAPIC_LINT0:
    return LAPIC_LVT_LINT0;
  case irq_LAPIC_LINT1:
    return LAPIC_LVT_LINT1;
  case irq_LAPIC_PCINT:
    return LAPIC_LVT_PCINT;
#endif
  case irq_LAPIC_IPI:
    return LAPIC_ICR0;
  default:
    fatal("Unknown LAPIC interrupt\n");
  }
}

static void
lapic_setup(VectorInfo *vi)
{
  uint32_t val;
  uint32_t reg = lapic_irq_register(vi->irq);

  switch(vi->irq) {
  case irq_LAPIC_SVR:
    {
      /* SVR, Timer do not have mode/level bits */
      val = lapic_read_register(LAPIC_SVR);
      val &= ~LAPIC_SVR_VECTOR_MASK;
      val = val | (vi - &VectorMap[0]) | LAPIC_SVR_ENABLED;
      lapic_write_register(LAPIC_SVR, val);
      break;
    }
  case irq_LAPIC_Timer:
    {
      /* SVR, Timer do not have mode/level bits */
      val = lapic_read_register(reg);
      val &= ~LAPIC_LVT_VECTOR_MASK;
      val = val | (vi - &VectorMap[0]);
      lapic_write_register(reg, val);
      printf("LVT TIMER (setup): 0x%08x\n", lapic_read_register(reg));
      break;
    }
#if 0
  case irq_LAPIC_ERROR:
    {
      /* ERROR LVT does not implement mode/active bits */
      val = lapic_read_register(LAPIC_LVT_ERROR);
      val &= ~LAPIC_LVT_VECTOR_MASK;
      val = val | (vi - &VectorMap[0]);
      lapic_write_register(LAPIC_LVT_Timer, val);
      break;
    }
#endif
  case irq_LAPIC_IPI:
    break;
  default:
    fatal("Cannot enable lapic IRQ 0x%x\n", vi->irq);
  }


  /* Already set up at initialization time. */
}

static void
lapic_unmask(VectorInfo *vi)
{
  switch(vi->irq) {
  case irq_LAPIC_SVR:
    /* Spurious vector interrupt. Cannot be enabled or disabled from software. */
    break;
  default:
    {
      uint32_t reg = lapic_irq_register(vi->irq);
      uint32_t val = lapic_read_register(reg);
      lapic_write_register(reg, val & ~LAPIC_LVT_MASKED);
      break;
    }
  }
}

static void
lapic_mask(VectorInfo *vi)
{
  switch(vi->irq) {
  case irq_LAPIC_SVR:
    /* Spurious vector interrupt. Cannot be enabled or disabled from software. */
    break;
  default:
    {
      uint32_t reg = lapic_irq_register(vi->irq);
      uint32_t val = lapic_read_register(reg);
      lapic_write_register(reg, val | LAPIC_LVT_MASKED);
      break;
    }
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
  .baseIRQ = 0,
  .nIRQ = 16,
  .va = 0,
  .setup = lapic_setup,
  .unmask = lapic_unmask,
  .mask = lapic_mask,
  .isPending = lapic_isPending,
  .ack = lapic_acknowledge,
};

void
lapic_spurious_interrupt()
{
  fatal("Spurious lapic interrupt!\n");
}

void 
lapic_dump()
{
  printf("LAPIC TIMER 0x%08x   LAPIC ERROR 0x%08x\n"
	 "LAPIC LINT0 0x%08x   LAPIC LINT1 0x%08x\n"
	 "LAPIC PCINT 0x%08x   LAPIC LDR   0x%08x\n"
	 "LAPIC DFR   0x%08x   LAPIC ESR   0x%08x\n"
	 "LAPIC ICR0  0x%08x   LAPIC ICR1  0x%08x\n"
	 "LAPIC SVR   0x%08x\n",
	 lapic_read_register(LAPIC_LVT_Timer),
	 lapic_read_register(LAPIC_LVT_ERROR),
	 lapic_read_register(LAPIC_LVT_LINT0),
	 lapic_read_register(LAPIC_LVT_LINT1),
	 lapic_read_register(LAPIC_LVT_PCINT),
	 lapic_read_register(LAPIC_LDR),
	 lapic_read_register(LAPIC_DFR),
	 lapic_read_register(LAPIC_ESR),
	 lapic_read_register(LAPIC_ICR0),
	 lapic_read_register(LAPIC_ICR32),
	 lapic_read_register(LAPIC_SVR));

  printf("ISR=0x");
  for (size_t i = 0; i < 0x80; i+= 0x10)
    printf("%08x", lapic_read_register(LAPIC_ISR+i));
  printf("\n");
  printf("IRR=0x");
  for (size_t i = 0; i < 0x80; i+= 0x10)
    printf("%08x", lapic_read_register(LAPIC_IRR+i));
  printf("\n");
  printf("TMR=0x");
  for (size_t i = 0; i < 0x80; i+= 0x10)
    printf("%08x", lapic_read_register(LAPIC_TMR+i));
  printf("\n");

}

/** @brief Initialize the LAPIC interrupt controller.
 */
void
lapic_init()
{
  if (lapic_requires_8259_disable) {
    /* Following disables all interrupts on the primary and secondary
     * 8259. Disabling secondary shouldn't be necessary, but that
     * assumes that the ASIC emulating the 8259 is sensible.
     */
    i8259_shutdown();
  }
    
  {
    VectorInfo *vector = irq_MapInterrupt(irq_LAPIC_Timer);
    vector->type = vt_Interrupt;
    vector->mode = VEC_MODE_FROMBUS;
    vector->level = VEC_LEVEL_FROMBUS;
    vector->irq = irq_LAPIC_Timer;
    vector->fn = vh_UnboundIRQ;
    vector->unmasked = 0;
    vector->ctrlr = &lapic;
  }

  {
    VectorInfo *vector = irq_MapInterrupt(irq_LAPIC_IPI);
    vector->type = vt_Interrupt;
    vector->mode = VEC_MODE_EDGE;
    vector->level = VEC_LEVEL_ACTLOW; /* ?? */
    vector->irq = irq_LAPIC_IPI;
    vector->fn = vh_UnboundIRQ;
    vector->unmasked = 0;
    vector->ctrlr = &lapic;
  }

  {
    VectorInfo *vector = irq_MapInterrupt(irq_LAPIC_SVR);
    vector->type = vt_Interrupt;
    vector->mode = VEC_MODE_FROMBUS;
    vector->level = VEC_LEVEL_FROMBUS; /* ??? */
    vector->irq = irq_LAPIC_SVR;
    vector->fn = vh_UnboundIRQ;
    vector->unmasked = 0;
    vector->ctrlr = &lapic;
  }

  irq_Bind(irq_LAPIC_SVR, VEC_MODE_EDGE, VEC_LEVEL_ACTHIGH, 
	   lapic_spurious_interrupt);

  VectorInfo *vector = irq_MapInterrupt(irq_LAPIC_SVR);
  vector->unmasked = 1;
  vector->ctrlr->unmask(vector);

  lapic_eoi();
}
