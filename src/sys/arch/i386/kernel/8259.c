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
 * @brief 8259 Peripheral interrupt controller support.
 */

#include <stddef.h>
#include <stdbool.h>
#include <hal/kerntypes.h>
#include <hal/irq.h>
#include <hal/machine.h>
#include <kerninc/printf.h>
#include <kerninc/assert.h>
#include <kerninc/CPU.h>
#include <kerninc/PhysMem.h>
#include <coyotos/i386/io.h>
#include "IRQ.h"
#include "cpu.h"
#include "8259.h"
#include "PIC.h"

/****************************************************************
 * 8259 SUPPORT
 ****************************************************************/

#ifdef BRING_UP
void irq_set_softled(uint32_t vec, bool on)
{
  if (vec >= 72)
    return;

#define BlackOnGreen      0x20
#define WhiteOnRed        0x47
#define BlackOnLightGreen 0xa0
#define WhiteOnLightRed   0xc7

  ((char *) 0xc00b8F00)[16 + 2*vec + 1] = 
    on ? BlackOnLightGreen : WhiteOnLightRed;
}
#endif

static uint16_t i8259_irqMask   = (uint16_t) ~0u;

/*******************************************************************************
 * For 8259 we implement a single pseudo-controller covering both
 * chips, because it is simpler that way.
 *******************************************************************************/
static void
i8259_setup(VectorInfo *vi)
{
  /* Already set up at initialization time. */
  assert(vi->mode == VEC_MODE_EDGE && vi->level == VEC_LEVEL_ACTHIGH);
}

static void
i8259_enable(VectorInfo *vector)
{
  assert(cpu_ncpu == 1);

  irq_t irq = vector->irq;
  assert(irq < 16);

  const size_t cascade_pin = IRQ_PIN(irq_ISA_Cascade);
  flags_t flags = locally_disable_interrupts();

  i8259_irqMask &= ~(1u << irq);
#ifdef BRING_UP
  irq_set_softled(vec_IRQ0 + irq, true);
#endif

  if (irq >= 8) {
    outb((i8259_irqMask >> 8), IO_8259_ICU2_IMR);

    /* If we are doing cascaded 8259s, any interrupt enabled on the
       second PIC requires that the cascade vector on the primary PIC
       be enabled. */
    if (i8259_irqMask & (1u << cascade_pin)) {
      i8259_irqMask &= ~(1u << cascade_pin);
#ifdef BRING_UP
      irq_set_softled(vec_IRQ0 + cascade_pin, true);
#endif
    }
  }

  if (irq < 8)
    outb(i8259_irqMask & 0xffu, IO_8259_ICU1_IMR);

  // printf("Enable IRQ %d\n", vector - NUM_TRAP);

  locally_enable_interrupts(flags);
}

static void
i8259_disable(VectorInfo *vector)
{
  assert(cpu_ncpu == 1);

  irq_t irq = vector->irq;
  assert(irq < 16);

  flags_t flags = locally_disable_interrupts();

  i8259_irqMask |= (1u << irq);
#ifdef BRING_UP
  irq_set_softled(vec_IRQ0 + irq, false);
#endif

  if (irq >= 8)
    outb((i8259_irqMask >> 8) & 0xffu, IO_8259_ICU2_IMR);
  else
    outb(i8259_irqMask & 0xffu, IO_8259_ICU1_IMR);

  // printf("Disable IRQ %d\n", vector - NUM_TRAP);

  locally_enable_interrupts(flags);
}

static bool
i8259_isPending(VectorInfo *vector)
{
  assert(cpu_ncpu == 1);

  irq_t irq = vector->irq;
  assert(irq < 16);

  flags_t flags = locally_disable_interrupts();
  bool isPending = false;	/* until proven otherwise */

  uint8_t bit = (1u << (irq & 0x7u));
  uint16_t ICU = (irq < 8) ? IO_8259_ICU1 : IO_8259_ICU2;

  outb(0xb, ICU);		/* read ISR */
  if (inb(ICU) & bit)
    isPending = true;

  locally_enable_interrupts(flags);

  return isPending;
}

static void
i8259_acknowledge(VectorInfo *vector)
{
  assert(cpu_ncpu == 1);

  irq_t irq = vector->irq;
  assert(irq < 16);

  flags_t flags = locally_disable_interrupts();

  // printf("Acknowledge IRQ0\n");

  if (irq >= 8)
    outb(0x20, IO_8259_ICU2);	/* Ack the int to ICU2 */

  outb(0x20, IO_8259_ICU1);	/* Ack the int to ICU1. Do this in all
				   cases because of cascade pin. */

  locally_enable_interrupts(flags);
}

static IrqController i8259 = {
  0,
  16,
  0,
  i8259_setup,
  i8259_enable,
  i8259_disable,
  i8259_isPending,
  i8259_acknowledge,
  pic_no_op,
};

/** @brief Initialize the PC motherboard legacy ISA peripheral
 *  interrupt controllers.
 */
void
i8259_init()
{
  assert(cpu_ncpu == 1);

  nGlobalIRQ = 16;

  /* Set up the vector entries */
  for (size_t pin = 0; pin < i8259.nIRQ; pin++) {
    irq_t irq = i8259.baseIRQ + pin;
    uint32_t vec = vec_IRQ0 + irq;
    IrqVector[irq] = &VectorMap[vec];
    VectorMap[vec].type = vt_Interrupt;
    VectorMap[vec].mode = VEC_MODE_FROMBUS; /* all legacy IRQs are edge triggered */
    VectorMap[vec].level = VEC_LEVEL_FROMBUS; /* all legacy IRQs are active high. */
    VectorMap[vec].irq = irq;
    VectorMap[vec].enabled = 0;
    VectorMap[vec].ctrlr = &i8259;
  }

  /* Set up the interrupt controller chip: */

  outb(0x11, IO_8259_ICU1);	/* ICW1: edge triggered */
  outb(NUM_TRAP, IO_8259_ICU1+1); /* ICW2: interrupts from 0x20 to 0x27 */
  outb(0x04, IO_8259_ICU1+1);	/* ICW3: cascade on IRQ2 */
  outb(0x01, IO_8259_ICU1+1);	/* ICW4: 8086 mode */

  /* OCW1: disable all interrupts on ICU1 */
  outb(i8259_irqMask & 0xffu, IO_8259_ICU1+1);

  outb(0x11, IO_8259_ICU2);	/* ICW1: edge triggered */
  outb(NUM_TRAP+8, IO_8259_ICU2+1);	/* ICW2: interrupts from 0x28 to 0x2f */
  outb(0x02, IO_8259_ICU2+1);	/* ICW3: slave ID 2  */
  outb(0x01, IO_8259_ICU2+1);	/* ICW4: 8086 mode */

  /* OCW1: disable all interrupts on ICU2 */
  outb((i8259_irqMask & 0xffu) >> 8 & 0xffu, IO_8259_ICU2+1); 

  outb(0x20, IO_8259_ICU1);	/* reset pic1 */
  outb(0x20, IO_8259_ICU2);	/* reset pic2 */

#ifdef BRING_UP
  for (size_t irq = 0; irq < 16; irq++)
    irq_set_softled(vec_IRQ0 + irq, false);
#endif
}

void
i8259_shutdown()
{
  outb(0xffu, IO_8259_ICU1_IMR);
  outb(0xffu, IO_8259_ICU2_IMR);
}
