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
 * @brief I/O APIC support.
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
#include "8259.h"
#include "cpu.h"
#include "acpi.h"

#define DEBUG_IOAPIC if (0)

/* I/O APIC Register numbers */
/** @brief I/O APIC Identitification Register */
#define IOAPIC_ID         0
#define   IOAPIC_ID_MASK        0x0f000000
#define   IOAPIC_ID_SHIFT       24
/** @brief I/O APIC Version Register */
#define IOAPIC_VERSION    1
#define   IOAPIC_VERSION_MASK   0xff
#define   IOAPIC_MAXREDIR_MASK  0x00ff0000
#define   IOAPIC_MAXREDIR_SHIFT 16
/** @brief I/O APIC Arbitration Register */
#define IOAPIC_ARB        2
#define   IOAPIC_ARB_ID_MASK     0x0f000000
#define   IOAPIC_ARB_ID_SHIFT    24
/** @brief I/O APIC Redirection Table */
#define IOAPIC_ARB        2

#define IOAPIC_ENTRYLO(n)        (0x10+(2*n))
#define IOAPIC_ENTRYHI(n)        (0x10+(2*n)+1)

typedef struct IoAPIC_Entry {
  union {
    struct {
      /** @brief Interrupt vector used for delivery (R/W) */
      uint32_t vector : 8;
      /** @brief Delivery mode (R/W).
       *
       * <table>
       * <tr><td>Value</td>
       *     <td>Mode</td>
       *     <td>Meaning</td></tr>
       * <tr><td>000</td>
       *     <td>Fixed</td>
       *     <td>Deliver signal on the INTR signal of all destination 
       *         processors.</td></tr>
       * <tr><td>001</td>
       *     <td>Lowest Priority</td>
       *     <td>Deliver signal on the INTR signal of the procesor
       *         core that is executing at lowest priority among
       *         destination processors.</td></tr>
       * <tr><td>010</td>
       *     <td>SMI</td>
       *     <td>System Management Interrupt. Requires edge trigger
       *         mode. Vector field ignored but should be zero.</td></tr>
       * <tr><td>011</td>
       *     <td><em>reserved</em></td></tr>
       * <tr><td>100</td>
       *     <td>NMI</td>
       *     <td>Deliver signal on the NMI signal of all destination
       *         processor cores. Vector info ignored. Always treated
       *         as edge triggered. Should be programmed that
       *         way.</td></tr> 
       * <tr><td>101</td>
       *     <td>INIT</td>
       *     <td>Deliver signal to all processor cores listed in the
       *         destination by asserting the INIT signal, causing
       *         them to enter INIT state. Always treated as edge
       *         triggered. Should be programmed that way.</tr>
       * <tr><td>110</td>
       *     <td><em>reserved</em></td></tr>
       * <tr><td>111</td>
       *     <td>ExtINT</td>
       *     <td>Deliver to INTR signal of all destination processors
       *         as an interrupt that originated from 8259A. Changes
       *         INTA cycle routing. Requires edge trigger
       *          mode.</td></tr>
       * </table>
       */
      uint32_t deliverMode : 3;

      /** @brief Destination mode (R/W).
       *
       * 0: physical, 1: logical
       */
      uint32_t destMode : 1;

      /** @brief Delivery Status (RO)
       *
       * 0: idle 1: send pending
       */
      uint32_t deliveryStatus: 1;

      /** @brief Interrupt pin polarity (R/W)
       *
       * 0: active high, 1: active low
       */
      uint32_t polarity : 1;

      /** @brief Remote IRR (RO)
       *
       * Defined only for level triggered interrupts. Set to 1 when
       * local APIC(s) accept the leve interrupt. Set to 0 when EOI
       * message with matching vector is received.
       *
       * Read-only
       */
      uint32_t remoteIRR : 1;

      /** @brief Trigger Mode (R/W)
       *
       * 1: level, 0: edge
       */
      uint32_t triggerMode : 1;

      /** @brief Masked (R/W)
       *
       * 1: masked, 0: enabled
       */
      uint32_t masked : 1;


      uint32_t  : 15; 
      uint32_t  : 24; 

      /** @brief if dest mode is physical, contains an APIC ID. If
       * dest mode is logical, specifies logical destination address,
       * which may be a @em set of processors.
       */
      uint32_t dest : 8;
    } fld;
    struct {
      uint32_t lo;
      uint32_t hi;
    } raw;
  } u;
} IoAPIC_Entry;

static void ioapic_enable(IrqController *ctrlr, irq_t irq);
static void ioapic_disable(IrqController *ctrlr, irq_t irq);
// static void ioapic_acknowledge(IrqController *ctrlr, irq_t irq);
static bool ioapic_isPending(IrqController *ctrlr, irq_t irq);

static IrqController ioapic[3];
static size_t nIoAPIC = 0;

void
ioapic_register(irq_t baseIRQ, kva_t va)
{
  if (nIoAPIC == (sizeof(ioapic) / sizeof(ioapic[0])))
    fatal("Too many I/O APICs\n");

  ioapic[nIoAPIC].baseIRQ = baseIRQ;
  ioapic[nIoAPIC].nIRQ = 0;	/* not yet known */
  ioapic[nIoAPIC].va = va;
  ioapic[nIoAPIC].enable = ioapic_enable;
  ioapic[nIoAPIC].disable = ioapic_disable;
  ioapic[nIoAPIC].isPending = ioapic_isPending;
  ioapic[nIoAPIC].earlyAck = pic_no_op;
  ioapic[nIoAPIC].lateAck = pic_no_op;


  nIoAPIC++;
}


static inline uint32_t
ioapic_read_reg(IrqController *ctrlr, uint32_t reg)
{
  /* data window is at ioapic_va + 0x10 */
  volatile uint32_t *va_reg = (uint32_t *) ctrlr->va;
  volatile uint32_t *va_data = (uint32_t *) (ctrlr->va + 0x10);

  *va_reg = reg;
  uint32_t val = *va_data;
  return val;
}

static inline void
ioapic_write_reg(IrqController *ctrlr, uint32_t reg, uint32_t val)
{
  /* data window is at ioapic_va + 0x10 */
  volatile uint32_t *va_reg = (uint32_t *) ctrlr->va;
  volatile uint32_t *va_data = (uint32_t *) (ctrlr->va + 0x10);

  *va_reg = reg;
  *va_data = val;
}

static inline IoAPIC_Entry
ioapic_read_entry(IrqController *ctrlr, uint32_t pin)
{
  IoAPIC_Entry ent;

  ent.u.raw.lo = ioapic_read_reg(ctrlr, IOAPIC_ENTRYLO(pin));
  ent.u.raw.hi = ioapic_read_reg(ctrlr, IOAPIC_ENTRYHI(pin));
  return ent;
}

static inline void
ioapic_write_entry(IrqController *ctrlr, uint32_t pin, IoAPIC_Entry ent)
{
  ioapic_write_reg(ctrlr, IOAPIC_ENTRYLO(pin), ent.u.raw.lo);
  ioapic_write_reg(ctrlr, IOAPIC_ENTRYHI(pin), ent.u.raw.hi);
}

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

static spinlock_t ioapic_lock;

static void
ioapic_enable(IrqController *ctrlr, irq_t irq)
{
  SpinHoldInfo shi = spinlock_grab(&ioapic_lock);
  size_t pin = irq - ctrlr->baseIRQ;

  IoAPIC_Entry e = ioapic_read_entry(ctrlr, pin);
  e.u.fld.masked = 0;
  ioapic_write_entry(ctrlr, pin, e);

  spinlock_release(shi);
}

static void
ioapic_disable(IrqController *ctrlr, irq_t irq)
{
  SpinHoldInfo shi = spinlock_grab(&ioapic_lock);
  size_t pin = irq - ctrlr->baseIRQ;

  IoAPIC_Entry e = ioapic_read_entry(ctrlr, pin);
  e.u.fld.masked = 1;
  ioapic_write_entry(ctrlr, pin, e);

  spinlock_release(shi);
}

#if 0
static void
ioapic_acknowledge(IrqController *ctrlr, irq_t irq)
{
  SpinHoldInfo shi = spinlock_grab(&ioapic_lock);

  bug("Cat can't acknowledge food in tin apics\n");

  spinlock_release(shi);
}
#endif

static bool
ioapic_isPending(IrqController *ctrlr, irq_t irq)
{
  bool result = false;

  SpinHoldInfo shi = spinlock_grab(&ioapic_lock);
  size_t pin = irq - ctrlr->baseIRQ;

  IoAPIC_Entry e = ioapic_read_entry(ctrlr, pin);
  if (e.u.fld.deliveryStatus)
    result = true;

  spinlock_release(shi);

  return result;
}

static void 
ioapic_ctrlr_init(IrqController *ctrlr)
{
  uint32_t id = ioapic_read_reg(ctrlr, IOAPIC_ID);
  uint32_t ver = ioapic_read_reg(ctrlr, IOAPIC_VERSION);
  uint32_t nInts = (ver & IOAPIC_MAXREDIR_MASK) >> IOAPIC_MAXREDIR_SHIFT;

  ctrlr->nIRQ = nInts;
  if (ctrlr->baseIRQ + nInts > nIRQ)
    nIRQ = ctrlr->baseIRQ + nInts;

  DEBUG_IOAPIC 
    printf("I/O APIC id is %d, ver %d, nInts %d\n", 
	   id >> IOAPIC_ID_SHIFT,
	   ver & IOAPIC_VERSION_MASK,
	   nInts);

  size_t vec = 0;

  /** @bug This is doing the wrong thing. We ought to be using the
   * ACPI interrupt sources table, and falling back to this if that
   * is missing. */
  /* Set up the vector entries */
  for (size_t pin = 0; pin < ctrlr->nIRQ; pin++) {
    irq_t irq = ctrlr->baseIRQ + pin;
    while(VectorMap[vec].type != vt_Unbound)
      vec++;

    IrqVector[irq] = &VectorMap[vec];
    VectorMap[vec].type = vt_Interrupt;
    VectorMap[vec].edge = (irq < 16);	/* all legacy IRQs are edge triggered */
    VectorMap[vec].irq = irq;
    VectorMap[vec].ctrlr = ctrlr;

    irq_Unbind(irq);
  }
}

void
ioapic_init()
{
  if (lapic_requires_8259_disable) {
    /* Following disables all interrupts on the primary and secondary
     * 8259. Disabling secondary shouldn't be necessary, but that
     * assumes that the ASIC emulating the 8259 is sensible.
     */
    i8259_shutdown();
  }
    
  for (size_t i = 0; i < nIoAPIC; i++)
    ioapic_ctrlr_init(&ioapic[i]);

  // Linux clears interrupts on the local APIC when switching. OpenBSD
  // does not. I suspect that Linux is doing this a defense against
  // sleep recovery. For the moment, don't do it.
    
  outb(IMCR_SET_INTERRUPT_MODE, IMCR);
  outb(IMCR_LAPIC_MODE, IMCR_DATA);

  // For each vector corresponding to a defined interrupt pin, wire
  // the pin back to that vector
  for (size_t vec = 0; vec < NUM_VECTOR; vec++) {
    if (VectorMap[vec].type != vt_Interrupt)
      continue;

    IrqController *ctrlr = VectorMap[vec].ctrlr;
    size_t pin = VectorMap[vec].irq - ctrlr->baseIRQ;

    IoAPIC_Entry e = ioapic_read_entry(ctrlr, pin);
    e.u.fld.vector = vec;
    e.u.fld.deliverMode = 0;	/* FIXED delivery */
    e.u.fld.destMode = 0;		/* Physical destination (for now) */
    e.u.fld.polarity = 0;		/* Active high */
    e.u.fld.triggerMode = VectorMap[vec].edge ? 0 : 1;
    e.u.fld.masked = 1;
    e.u.fld.dest = archcpu_vec[0].lapic_id; /* CPU0 for now */

    ioapic_write_entry(ctrlr, pin, e);

    DEBUG_IOAPIC {
      irq_t irq = ctrlr->baseIRQ + pin;
      printf("Vector %d -> irq %d  ", e.u.fld.vector, irq);
      if ((irq % 2) == 1)
	printf("\n");

      IoAPIC_Entry e2 = ioapic_read_entry(ctrlr, pin);
      if (e2.u.fld.vector != e.u.fld.vector)
	fatal("e.vector %d e2.vector %d\n",
	      e.u.fld.vector, e2.u.fld.vector);
    }
  }
  DEBUG_IOAPIC printf("\n");

  DEBUG_IOAPIC {
    for (size_t irq = 0; irq < nIRQ; irq++) {
      VectorInfo *vector = IrqVector[irq];
      size_t pin = irq - vector->ctrlr->baseIRQ;
      IoAPIC_Entry e = ioapic_read_entry(vector->ctrlr, pin);
      printf("IRQ %3d -> vector %d  ", 
	     irq, e.u.fld.vector);
      if ((irq % 2) == 1)
	printf("\n");
    }
    if ((nIRQ % 2) == 1)
      printf("\n");

    fatal("Check map.\n");
  }
}

void
ioapic_shutdown()
{
  // NOTE: This is untested and probably does not work!
  fatal("Do not know how to perform LAPIC shutdown.\n");
  outb(IMCR_SET_INTERRUPT_MODE, IMCR);
  outb(IMCR_PIC_MODE, IMCR_DATA);
}
