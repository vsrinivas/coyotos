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
#include <kerninc/PhysMem.h>
#include <coyotos/i386/io.h>
#include "IRQ.h"
#include "PIC.h"

bool lapic_works = false;
bool lapic_requires_8259_disable = false;
kpa_t lapic_pa = 0;		/* if present, certainly won't be here */
volatile uint32_t *lapic_va = 0;

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

#define IO_8259_ICU1 0x20
#define IO_8259_ICU1_IMR 0x21
#define IO_8259_ICU2 0xA0
#define IO_8259_ICU2_IMR 0xA1

/* The now-obsolete Intel Multiprocessor Specification introduces an
 * Interrupt Mode Control Register, which is used to get the chipset
 * to re-arrange the interrupt lines back and forth between the legacy
 * interrupt controller and the local APIC. Curiously, there is no
 * mention of any such requirement in later ACPI specs.
 *
 * Protocol: write the constant 0x70 to the IMCR (port 22), then write
 * the desired mode to port 23.
 */
#define IMCR 0x22
#define IMCR_DATA 0x23
#define IMCR_SET_INTERRUPT_MODE 0x70
#define IMCR_PIC_MODE 0
#define IMCR_LAPIC_MODE 1

/* Definitions of LAPIC-related registers and values */
#define LAPIC_ID         0x020
#define LAPIC_VERSION    0x030
#define LAPIC_TPR        0x080	/* Task Priority Register */
#define LAPIC_APR        0x090	/* Arbitration Priority Register */
#define LAPIC_PPR        0x0A0	/* Processor Priority Register */
#define LAPIC_EOI        0x0B0	/* EIO Register */
#define LAPIC_LDR        0x0D0	/* Logical Destination Register */
#define LAPIC_DFR        0x0E0	/* Destination Format Register */
#define LAPIC_SVR        0x0F0	/* Spurious Interrupt Vector Register */
#define   LAPIC_SVR_VECTOR_MASK  0x0ffu
#define   LAPIC_SVR_ENABLE       0x100u
#define   LAPIC_SVR_FOCUS        0x200u
#define LAPIC_ISR        0x100	/* In-Service Register 0-255 */
#define LAPIC_TMR        0x180	/* Trigger-Mode Register 0-255 */
#define LAPIC_IRR        0x200	/* Interrupt Request Register 0-255 */
#define LAPIC_ESR        0x280	/* Error Status Register */
#define LAPIC_ICR0       0x300	/* Interrupt Command Register 0-31 */
#define LAPIC_ICR32      0x310	/* Interrupt Command Register 32-63 */
#define LAPIC_LVT_Timer  0x320	/* Local Vector Table (Timer) */
#define LAPIC_PerfCntr   0x340	/* Performance Counter */
#define LAPIC_LVT_LINT0  0x350	/* Local Vector Table (LINT0) */
#define LAPIC_LVT_LINT1  0x360	/* Local Vector Table (LINT1) */
#define LAPIC_LVT_ERROR  0x370	/* Local Vector Table (Error) */
#define LAPIC_Timer_InitCount  0x380	/* Initial Count Register for Timer */
#define LAPIC_Timer_CurCount   0x390	/* Current Count Register for Timer */
#define LAPIC_Timer_DivideCfg  0x3E0	/* Timer Divide Configuration Register */

/** @brief Vector used for spurious LAPIC interrupts.
 *
 * This vector is used for an interrupt which was aborted becauase the
 * cpu masked it after it happened but before it was
 * delivered. Low-order 4 bits must be 0xf.
 */
#define   LAPIC_SPURIOUS_VECTOR  0xefu

/** @brief Vector used for inter-processor-interrupts
 */
#define   LAPIC_IPI_VECTOR       /* ?? */

/** @brief Vector used for local apic timer interrupts
 */
#define   LAPIC_TIMER_VECTOR       /* ?? */


void
lapic_write_reg(uint32_t reg, uint32_t val)
{
  assert((reg % sizeof(*lapic_va)) == 0);

  *((volatile uint32_t *) (lapic_va + (reg / sizeof(*lapic_va)))) = val;

  /* Xeon errata: follow up with read from ID register, forcing above
     write to have observable effect. */
  val = *((volatile uint32_t *) (lapic_va + (LAPIC_ID / sizeof(*lapic_va))));

}

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
    irq_set_softled(iv_IRQ0 + irq, false);
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
  irq_set_softled(vector, true);
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
      irq_set_softled(iv_8259_Cascade, true);
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
  irq_set_softled(vector, false);
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
  pmem_AllocRegion(0XFEE00000, 0XFEE01000, pmc_DEV, pmu_DEV, 
		   "LAPIC region");

  lapic_va = (volatile uint32_t *) HEAP_LIMIT_VA;
  kmap_EnsureCanMap(HEAP_LIMIT_VA, "lapic");
  kmap_map(HEAP_LIMIT_VA, lapic_pa, KMAP_R|KMAP_W|KMAP_NC);

  if (lapic_requires_8259_disable)
    /* Following disables all interrupts on the master
     * 8259. Interrupts may still occur on the secondary, but we will not
     * see them because the cascade interrupt is disabled on the
     * primary.
     */
    outb(0xffu, IO_8259_ICU1_IMR);
    
  // Linux clears interrupts on the local APIC when switching. OpenBSD
  // does not. I suspect that Linux is doing this a defense against
  // sleep recovery. For the moment, don't do it.

  outb(IMCR_SET_INTERRUPT_MODE, IMCR);
  outb(IMCR_LAPIC_MODE, IMCR_DATA);

  lapic_write_reg(LAPIC_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VECTOR);
}

static void
lapic_shutdown()
{
  // Restore to PIC mode:
  outb(IMCR_SET_INTERRUPT_MODE, IMCR);
  outb(IMCR_PIC_MODE, IMCR_DATA);
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
  if (lapic_works && lapic_pa)
    return true;

  return false;
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

bool
pic_isPending(uint32_t vector)
{
  if (pic_have_lapic())
    return lapic_isPending(vector);
  else
    return i8259_isPending(vector);
}
