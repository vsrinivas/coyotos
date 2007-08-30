/*
 * Copyright (C) 2007, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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
 * @brief Peripheral interrupt controller support.
 *
 * This support is minimal, since we don't handle any of the fancy
 * APIC modes.
 */

#include <stddef.h>
#include <stdbool.h>
#include <hal/kerntypes.h>
#include <hal/irq.h>
#include <hal/machine.h>
#include <kerninc/printf.h>
#include <kerninc/assert.h>
#include <kerninc/CPU.h>
#include <coyotos/i386/io.h>
#include "IRQ.h"

bool lapic_requires_8259_disable = false;
kpa_t lapic_pa = 0;		/* ??? */

/****************************************************************
 * 8259 SUPPORT
 ****************************************************************/

#ifdef BRING_UP
void irq_set_led(uint32_t vec, bool on)
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

#define IO_8259_ICU1 0x20
#define IO_8259_ICU1_IMR 0x21
#define IO_8259_ICU2 0xA0
#define IO_8259_ICU2_IMR 0xA1

/** @brief Initialize the PC motherboard legacy ISA peripheral
 *  interrupt controllers.
 */
static void
i8259_init()
{
  assert(cpu_ncpu == 1);

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
    irq_set_led(iv_IRQ0 + irq, false);
#endif
}

void
i8259_enable(uint32_t vector)
{
  assert(cpu_ncpu == 1);
  assert((vector - NUM_TRAP) < 16);

  flags_t flags = locally_disable_interrupts();
  uint32_t irq = vector - NUM_TRAP;

  i8259_irqMask &= ~(1u << irq);
#ifdef BRING_UP
  irq_set_led(vector, true);
#endif

  if (irq >= 8) {
    outb((i8259_irqMask >> 8), IO_8259_ICU2_IMR);

    /* If we are doing cascaded 8259s, any interrupt enabled on the
       second PIC requires that the cascade vector on the primary PIC
       be enabled. */
    if (i8259_irqMask & (1u << (iv_8259_Cascade - NUM_TRAP))) {
      irq = iv_8259_Cascade - NUM_TRAP;
      i8259_irqMask &= ~(1u << irq);
#ifdef BRING_UP
      irq_set_led(iv_8259_Cascade, true);
#endif
    }
  }

  if (irq < 8)
    outb(i8259_irqMask & 0xffu, IO_8259_ICU1_IMR);

  // printf("Enable IRQ %d\n", vector - NUM_TRAP);

  locally_enable_interrupts(flags);
}

void
i8259_disable(uint32_t vector)
{
  assert(cpu_ncpu == 1);
  assert((vector - NUM_TRAP) < 16);

  flags_t flags = locally_disable_interrupts();
  uint32_t irq = vector - NUM_TRAP;

  i8259_irqMask |= (1u << irq);
#ifdef BRING_UP
  irq_set_led(vector, false);
#endif

  if (irq >= 8)
    outb((i8259_irqMask >> 8) & 0xffu, IO_8259_ICU2_IMR);
  else
    outb(i8259_irqMask & 0xffu, IO_8259_ICU1_IMR);

  // printf("Disable IRQ %d\n", vector - NUM_TRAP);

  locally_enable_interrupts(flags);
}

bool
i8259_isPending(uint32_t vector)
{
  assert(cpu_ncpu == 1);
  assert((vector - NUM_TRAP) < 16);

  flags_t flags = locally_disable_interrupts();
  uint32_t irq = vector - NUM_TRAP;
  bool isPending = false;	/* until proven otherwise */

  uint8_t bit = (1u << (irq & 0x7u));
  uint16_t ICU = (irq < 8) ? IO_8259_ICU1 : IO_8259_ICU2;

  outb(0xb, ICU);		/* read ISR */
  if (inb(ICU) & bit)
    isPending = true;

  locally_enable_interrupts(flags);

  return isPending;
}

void
i8259_acknowledge(uint32_t vector)
{
  assert(cpu_ncpu == 1);
  assert((vector - NUM_TRAP) < 16);

  flags_t flags = locally_disable_interrupts();
  uint32_t irq = vector - NUM_TRAP;

  // printf("Acknowledge IRQ0\n");

  if (irq >= 8)
    outb(0x20, IO_8259_ICU2);	/* Ack the int to ICU2 */

  outb(0x20, IO_8259_ICU1);	/* Ack the int to ICU1. Do this in all
				   cases because of cascade pin. */

  locally_enable_interrupts(flags);
}

/****************************************************************
 * LAPIC SUPPORT
 ****************************************************************/

static void
lapic_init()
{
  bug("Cat can't open food in tin lapics\n");
}

static void
lapic_shutdown()
{
  bug("Cat can't shutdown food in tin lapics\n");
}

void
lapic_enable(uint32_t vector)
{
  bug("Cat can't enable food in tin lapics\n");
}

void
lapic_disable(uint32_t vector)
{
  bug("Cat can't disable food in tin lapics\n");
}

void
lapic_acknowledge(uint32_t vector)
{
  bug("Cat can't acknowledge food in tin lapics\n");
}

bool
lapic_isPending(uint32_t vector)
{
  bug("Cat can't isPending food in tin lapics\n");
}



/****************************************************************
 * GENERIC WRAPPERS
 ****************************************************************/

bool
pic_have_lapic()
{
  return false;
  // return (lapic_pa != 0);
}

void
pic_init()
{
  if (pic_have_lapic())
    lapic_init();
  else
    i8259_init();
}

void
pic_shutdown()
{
  (void) locally_disable_interrupts();

  if (pic_have_lapic())
    lapic_shutdown();
}

void
pic_enable(uint32_t vector)
{
  if (pic_have_lapic())
    lapic_enable(vector);
  else
    i8259_enable(vector);
}

void
pic_disable(uint32_t vector)
{
  if (pic_have_lapic())
    lapic_disable(vector);
  else
    i8259_disable(vector);
}

void
pic_acknowledge(uint32_t vector)
{
  if (pic_have_lapic())
    lapic_acknowledge(vector);
  else
    i8259_acknowledge(vector);
}

void
pic_isPending(uint32_t vector)
{
  if (pic_have_lapic())
    lapic_isPending(vector);
  else
    i8259_isPending(vector);
}
