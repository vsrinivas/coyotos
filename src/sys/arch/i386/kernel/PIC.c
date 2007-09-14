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
#include "cpu.h"

bool use_apic = false;
bool lapic_requires_8259_disable = false;

kpa_t lapic_pa = 0;		/* if present, certainly won't be here */
volatile uint32_t *lapic_va = 0;
kpa_t ioapic_pa = 0;		/* if present, certainly won't be here */
volatile uint32_t *ioapic_va = 0;

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
      uint64_t vector : 8;
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
      uint64_t deliverMode : 3;

      /** @brief Destination mode (R/W).
       *
       * 0: physical, 1: logical
       */
      uint64_t destMode : 1;

      /** @brief Delivery Status (RO)
       *
       * 0: idle 1: send pending
       */
      uint64_t deliveryStatus: 1;

      /** @brief Interrupt pin polarity (R/W)
       *
       * 0: active high, 1: active low
       */
      uint64_t polarity : 1;

      /** @brief Remote IRR (RO)
       *
       * Defined only for level triggered interrupts. Set to 1 when
       * local APIC(s) accept the leve interrupt. Set to 0 when EOI
       * message with matching vector is received.
       *
       * Read-only
       */
      uint64_t remoteIRR : 1;

      /** @brief Trigger Mode (R/W)
       *
       * 1: level, 0: edge
       */
      uint64_t triggerMode : 1;

      /** @brief Masked (R/W)
       *
       * 1: masked, 0: enabled
       */
      uint64_t masked : 1;


      uint64_t reserved : 39; 

      /** @brief if dest mode is physical, contains an APIC ID. If
       * dest mode is logical, specifies logical destination address,
       * which may be a @em set of processors.
       */
      uint64_t dest : 8;
    } fld;
    struct {
      uint32_t lo;
      uint32_t hi;
    } raw;
  } u;
} IoAPIC_Entry;

static inline uint32_t
ioapic_read_reg(uint32_t reg)
{
  *((volatile uint32_t *) ioapic_va) = reg;
  uint32_t val = *((volatile uint32_t *) (ioapic_va + 4));
  return val;
}

static inline void
ioapic_write_reg(uint32_t reg, uint32_t val)
{
  *((volatile uint32_t *) ioapic_va) = reg;
  *((volatile uint32_t *) (ioapic_va + 4)) = reg;
}

static inline IoAPIC_Entry
ioapic_read_entry(uint32_t irq)
{
  IoAPIC_Entry ent;
  printf("Read %d\n", IOAPIC_ENTRYLO(irq));
  ent.u.raw.lo = ioapic_read_reg(IOAPIC_ENTRYLO(irq));
  printf("Read %d\n", IOAPIC_ENTRYHI(irq));
  ent.u.raw.hi = ioapic_read_reg(IOAPIC_ENTRYHI(irq));
  return ent;
}

static inline void
ioapic_write_entry(uint32_t irq, IoAPIC_Entry ent)
{
  printf("Write %d\n", IOAPIC_ENTRYLO(irq));
  ioapic_write_reg(IOAPIC_ENTRYLO(irq), ent.u.raw.lo);
  printf("Write %d\n", IOAPIC_ENTRYHI(irq));
  ioapic_write_reg(IOAPIC_ENTRYHI(irq), ent.u.raw.hi);
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

/* Definitions of LAPIC-related registers and values */
/** @brief LAPIC ID register.
 *
 * Physical identity of this LAPIC. */
#define LAPIC_ID            0x020
#define   LAPIC_ID_MASK                0x0f000000
#define   LAPIC_ID_SHIFT               24
/** @brief LAPIC Version Register.
 *
 * Identifies APIC version. MAXLVT field indicates number of LVT
 * entries. PPro had 5 LVT entries, and we don't use more than that
 * in any case. */
#define LAPIC_VERSION       0x030
#define   LAPIC_VERSION_VERSION_MASK   0x000000ff
#define     LAPIC_VERSION_LAPIC        0
#define     LAPIC_VERSION_i8289DX      1 /* external LAPIC chip */
#define   LAPIC_VERSION_MAXLVT_MASK    0x00ff0000
#define   LAPIC_VERSION_MAXLVT_SHIFT   16
/** @brief LAPIC Task Priority Register */
#define LAPIC_TPR           0x080
#define   LAPIC_TPR_PRIO               0xff
/** @brief LAPIC Arbitration Priority Register */
#define LAPIC_APR           0x090
#define   LAPIC_ZPR_PRIO               0xff
/** @brief LAPIC Processor Priority Register */
#define LAPIC_PPR           0x0A0
#define   LAPIC_PPR_PRIO               0xff
/** @brief LAPIC EOI Register
 *
 * "During interrupt service routine, software shoudl indicate acceptance of
 * lowest-priority, fixed, timer, and error interrupts by writing an
 * arbitrary vaelu into EOI register. This allows APIC to issue next
 * interrupt whether ISR has terminated ot not. */
#define LAPIC_EOI           0x0B0
/** @brief LAPIC Logical Destination Register */
#define LAPIC_LDR           0x0D0
#define   LAPIC_LDR_MASK               0xff000000
#define   LAPIC_LDR_SHIFT              24
/** @brief LAPIC Destination Format Register */
#define LAPIC_DFR           0x0E0
#define   LAPIC_DFR_MASK               0xf0000000
#define   LAPIC_DFR_SHIFT              28
/** @brief LAPIC Spurious Interrupt Vector Register. */
#define LAPIC_SVR           0x0F0
#define   LAPIC_SVR_VECTOR_MASK        0x0ffu
#define   LAPIC_SVR_ENABLED            0x100u
#define   LAPIC_SVR_FOCUS              0x200u
/** @brief LAPIC In-Service Register 0-255
 *
 * Bit set when APIC accepts interrupt, cleared when the INTA cycle
 * is issued */
#define LAPIC_ISR           0x100
/** @brief LAPIC Trigger Mode Register 0-255
 *
 * On acceptance, bit is set to 1 IFF level triggered. If level
 * triggered, EOI is sent to all I/O APICS when soft EOI is issued. */
#define LAPIC_TMR           0x180
/** @brief LAPIC Interrupt Request Register 0-255 
 *
 * Bit set to 1 when interrupt is delivered to processor but not
 * fully serviced yet (EOI not yet received). Highest priority bit
 * will be set during INTA cycle, and cleared during EIO cycle.*/
#define LAPIC_IRR           0x200
/** @brief LAPIC Error Status Register */
#define LAPIC_ESR           0x280
#define   LAPIC_ESR_Send_CS_Error             0x01
#define   LAPIC_ESR_Receive_CS_Error          0x02
#define   LAPIC_ESR_Send_Accept_Error         0x04
#define   LAPIC_ESR_Receive_Accept_Error      0x08
          /* 0x10 reserved */
#define   LAPIC_ESR_Send_Illegal_Vector       0x20
#define   LAPIC_ESR_Receive_Illegal_Vector    0x40
#define   LAPIC_ESR_Illegal_Register_Address  0x80
/** @brief LAPIC Interrupt Command Register 0-31 */
#define LAPIC_ICR0          0x300
#define   LAPIC_ICR0_VECTOR_MASK       0x000ff
#define   LAPIC_ICR0_DELIVER_MODE      0x00700
#define     LAPIC_ICR0_DELIVER_FIXED     0x00000
#define     LAPIC_ICR0_DELIVER_LOWPRIO   0x00100
#define     LAPIC_ICR0_DELIVER_SMI       0x00200
#define     LAPIC_ICR0_DELIVER_NMI       0x00400
#define     LAPIC_ICR0_DELIVER_INIT      0x00500
#define     LAPIC_ICR0_DELIVER_STARTUP   0x00600
#define   LAPIC_ICR0_DEST_MODE         0x00800
#define     LAPIC_ICR0_DESTMODE_PHYSICAL 0x00000
#define     LAPIC_ICR0_DESTMODE_LOGICAL  0x00800
#define   LAPIC_ICR0_DELIVER_PENDING   0x01000
#define   LAPIC_ICR0_LEVEL_ASSERT      0x04000
#define   LAPIC_ICR0_TRIGGER_MODE      0x08000
#define     LAPIC_ICR0_TRIGGER_EDGE      0x00000
#define     LAPIC_ICR0_TRIGGER_LEVEL     0x08000
#define   LAPIC_ICR0_DEST_SHORTHAND    0xC0000
#define     LAPIC_ICR0_DEST_FIELD        0x00000
#define     LAPIC_ICR0_DEST_SELF         0x40000
#define     LAPIC_ICR0_ALL_AND_SELF      0x80000
#define     LAPIC_ICR0_ALL_BUT_SELF      0xC0000
/** @brief LAPIC Interrupt Command Register 32-63 */
#define LAPIC_ICR32         0x310
/** @brief LAPIC Local Vector Table (Timer) */
#define LAPIC_LVT_Timer     0x320
/** @brief LAPIC Performance Counter */
#define LAPIC_LVT_PerfCntr  0x340
/** @brief LAPIC Local Vector Table (LINT0) */
#define LAPIC_LVT_LINT0     0x350
/** @brief LAPIC Local Vector Table (LINT1) */
#define LAPIC_LVT_LINT1     0x360
/** @brief LAPIC Local Vector Table (Error) */
#define LAPIC_LVT_ERROR     0x370
/** @brief vector number in LVT entry. */
#define   LAPIC_LVT_VECTOR_MASK       0x000ff
/** @brief Interrupt delivery mode.
 *
 * 000: fixed, 100: NMI, 111: External interrupt
 *
 * Present in LINT0, LINT1, PCINT only 
 */ 
#define   LAPIC_LVT_DELIVER_MODE      0x00700
#define     LAPIC_LVT_DELIVER_FIXED   0x00000
#define     LAPIC_LVT_DELIVER_NMI     0x00400
#define     LAPIC_LVT_DELIVER_ExtINT  0x00700
/* Bit 11 not defined */
/** @brief Interrupt delivery status
 *
 * 0: idle, 1: pending.
 */
#define   LAPIC_LVT_DELIVER_PENDING   0x01000
/** @brief Interrupt pin polarity.
 *
 * 0: active high, 1: active low
 *
 * LINT0, LINT1 only
 */
#define   LAPIC_LVT_POLARITY          0x02000
/** @brief Remote interrupt request register
 *
 * Undefined for edge-triggered interrupts. Set when local APIC
 * accepts the interrupt. Reset when an EOI command is received from
 * the processor.
 *
 * LINT0, LINT1 only
 */
#define   LAPIC_LVT_REMOTE_IRR        0x04000
/** @brief Interrupt trigger mode
 *
 * Applies when delivery mode is set to "fixed".
 *
 * LINT0, LINT1 only.
 */
#define   LAPIC_LVT_TRIGGER_MODE      0x08000
#define     LAPIC_LVT_TRIGGER_EDGE    0
#define     LAPIC_LVT_TRIGGER_LEVEL   1
/** @brief Interrupt delivery masked
 *
 * 0: unmasked, 1: masked.
 */
#define   LAPIC_LVT_MASKED            0x10000
/** @brief LVT Timer mode
 *
 * 0: one-shot, 1: periodic
 */
#define   LAPIC_LVT_TIMER_MODE        0x20000
#define     LAPIC_LVT_TIMER_ONESHOT   0x00000
#define     LAPIC_LVT_TIMER_PERIODIC  0x20000

/** @brief Initial Count Register */
#define LAPIC_ICR_TIMER     0x380
/** @brief Current Count Register */
#define LAPIC_CCR_TIMER     0x390
/** @brief Divide Configuration Register */
#define LAPIC_DCR_TIMER     0x3E0
#define   LAPIC_DCR_DIVIDE_VALUE_MASK  0xf
#define   LAPIC_DCR_DIVIDE_DIV2        0x0
#define   LAPIC_DCR_DIVIDE_DIV4        0x1
#define   LAPIC_DCR_DIVIDE_DIV8        0x2
#define   LAPIC_DCR_DIVIDE_DIV16       0x3
#define   LAPIC_DCR_DIVIDE_DIV32       0x4
#define   LAPIC_DCR_DIVIDE_DIV64       0x5
#define   LAPIC_DCR_DIVIDE_DIV128      0x6
#define   LAPIC_DCR_DIVIDE_DIV1        0x7

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


static inline uint32_t
lapic_read_reg(uint32_t reg)
{
  uint32_t val = 
    *((volatile uint32_t *) (lapic_va + (LAPIC_ID / sizeof(*lapic_va))));
  return val;
}

static inline void
lapic_write_reg(uint32_t reg, uint32_t val)
{
  assert((reg % sizeof(*lapic_va)) == 0);

  *((volatile uint32_t *) (lapic_va + (reg / sizeof(*lapic_va)))) = val;

  /* Xeon errata: follow up with read from ID register, forcing above
     write to have observable effect: */
  (void) lapic_read_reg(LAPIC_ID);
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
    irq_set_softled(vec_IRQ0 + irq, false);
#endif
}

void
i8259_enable(uint32_t irq)
{
  assert(cpu_ncpu == 1);
  assert(irq < 16);

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
    if (i8259_irqMask & (1u << irq_Cascade)) {
      irq = irq_Cascade;
      i8259_irqMask &= ~(1u << irq);
#ifdef BRING_UP
      irq_set_softled(vec_IRQ0 + irq_Cascade, true);
#endif
    }
  }

  if (irq < 8)
    outb(i8259_irqMask & 0xffu, IO_8259_ICU1_IMR);

  // printf("Enable IRQ %d\n", vector - NUM_TRAP);

  locally_enable_interrupts(flags);
}

void
i8259_disable(uint32_t irq)
{
  assert(cpu_ncpu == 1);
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

bool
i8259_isPending(uint32_t irq)
{
  assert(cpu_ncpu == 1);
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

void
i8259_acknowledge(uint32_t irq)
{
  assert(cpu_ncpu == 1);
  assert(irq < 16);

  flags_t flags = locally_disable_interrupts();

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

static spinlock_t ioapic_lock;

static void
apic_init()
{
  /* Only want to do this part on CPU 0 */
  if (lapic_va == 0) {
    /* In theory, this runs at startup time while interrupts are not
       yet enabled, but it is good to handle the lock consistently,
       and the "interrupts are disabled" assumption may change when we
       handle sleep states. */
    SpinHoldInfo shi = spinlock_grab(&ioapic_lock);

    kmap_EnsureCanMap(I386_LOCAL_APIC_VA, "lapic");
    kmap_EnsureCanMap(I386_IO_APIC_VA, "ioapic");
    kmap_map(I386_LOCAL_APIC_VA, lapic_pa, KMAP_R|KMAP_W|KMAP_NC);
    kmap_map(I386_IO_APIC_VA, ioapic_pa, KMAP_R|KMAP_W|KMAP_NC);

    lapic_va = (volatile uint32_t *) I386_LOCAL_APIC_VA;
    ioapic_va = (volatile uint32_t *) I386_IO_APIC_VA;

    if (lapic_requires_8259_disable) {
      /* Following disables all interrupts on the primary and secondary
       * 8259. Disabling secondary shouldn't be necessary, but that
       * assumes that the ASIC emulating the 8259 is sensible.
       */
      outb(0xffu, IO_8259_ICU1_IMR);
      outb(0xffu, IO_8259_ICU2_IMR);
    }
    
    // Linux clears interrupts on the local APIC when switching. OpenBSD
    // does not. I suspect that Linux is doing this a defense against
    // sleep recovery. For the moment, don't do it.
    
    outb(IMCR_SET_INTERRUPT_MODE, IMCR);
    outb(IMCR_LAPIC_MODE, IMCR_DATA);

    uint32_t id = ioapic_read_reg(IOAPIC_ID);
    uint32_t ver = ioapic_read_reg(IOAPIC_VERSION);
    uint32_t nInts = (ver & IOAPIC_MAXREDIR_MASK) >> IOAPIC_MAXREDIR_SHIFT;

    printf("I/O APIC id is %d, ver %d, nInts %d\n", 
	   id >> IOAPIC_ID_SHIFT,
	   ver & IOAPIC_VERSION_MASK,
	   nInts);

    // For each vector corresponding to a defined interrupt pin, wire
    // the pin back to that vector
    for (size_t vec = 0; vec < NUM_VECTOR; vec++) {
      if (VectorMap[vec].type != vt_Interrupt)
	continue;

      uint32_t irq = VectorMap[vec].irqSource;
      if (irq > nInts) {
	VectorMap[vec].irqSource = 255;
	continue;
      }

      IoAPIC_Entry e = ioapic_read_entry(irq);
      e.u.fld.vector = vec;
      e.u.fld.deliverMode = 0;	/* FIXED delivery */
      e.u.fld.destMode = 0;		/* Physical destination (for now) */
      e.u.fld.polarity = 0;		/* Active high */
      e.u.fld.triggerMode = VectorMap[vec].edge ? 0 : 1;
      e.u.fld.masked = 1;
      e.u.fld.dest = archcpu_vec[0].lapic_id; /* CPU0 for now */

      printf("Vector %d -> irq %d  ", e.u.fld.vector, irq);
      if ((irq % 2) == 1)
	printf("\n");
      ioapic_write_entry(irq, e);

      IoAPIC_Entry e2 = ioapic_read_entry(irq);
      if (e2.u.fld.vector != e.u.fld.vector)
	fatal("e.vector %d e2.vector %d\n",
	       e.u.fld.vector, e2.u.fld.vector);
    }
    printf("\n");

    for (size_t irq = 0; irq < nInts; irq++) {
      IoAPIC_Entry e = ioapic_read_entry(irq);
      printf("IRQ %3d -> vector %d  ", 
	     irq, e.u.fld.vector);
      if ((irq % 2) == 1)
	printf("\n");
    }
    if ((nInts % 2) == 1)
      printf("\n");

    fatal("Check map.\n");
    spinlock_release(shi);
  }

  lapic_write_reg(LAPIC_SVR, LAPIC_SVR_ENABLED | LAPIC_SPURIOUS_VECTOR);
}

static void
apic_shutdown()
{
  // NOTE: This is untested and probably does not work!
  fatal("Do not know how to perform LAPIC shutdown.\n");
  outb(IMCR_SET_INTERRUPT_MODE, IMCR);
  outb(IMCR_PIC_MODE, IMCR_DATA);
}

void
apic_enable(uint32_t irq)
{
  SpinHoldInfo shi = spinlock_grab(&ioapic_lock);

  IoAPIC_Entry e = ioapic_read_entry(irq);
  e.u.fld.masked = 0;
  ioapic_write_entry(irq, e);

  spinlock_release(shi);
}

void
apic_disable(uint32_t irq)
{
  SpinHoldInfo shi = spinlock_grab(&ioapic_lock);

  IoAPIC_Entry e = ioapic_read_entry(irq);
  e.u.fld.masked = 1;
  ioapic_write_entry(irq, e);

  spinlock_release(shi);
}

void
apic_acknowledge(uint32_t irq)
{
  SpinHoldInfo shi = spinlock_grab(&ioapic_lock);

  bug("Cat can't acknowledge food in tin apics\n");

  spinlock_release(shi);
}

bool
apic_isPending(uint32_t irq)
{
  bool result = false;

  SpinHoldInfo shi = spinlock_grab(&ioapic_lock);

  IoAPIC_Entry e = ioapic_read_entry(irq);
  if (e.u.fld.deliveryStatus)
    result = true;

  spinlock_release(shi);

  return result;
}



/****************************************************************
 * GENERIC WRAPPERS
 ****************************************************************/

bool
pic_have_apic()
{
  if (use_apic == false)
    return false;

  if (lapic_pa && ioapic_pa)
    return true;

  use_apic = false;
  return false;
}

void
pic_init()
{
  if (pic_have_apic()) {
    apic_init();
    printf("APIC, ");
  }
  else {
    i8259_init();
    printf("PIC, ");
  }
}

void
pic_shutdown()
{
  (void) locally_disable_interrupts();

  if (pic_have_apic())
    apic_shutdown();
}

void
pic_enable(uint32_t irq)
{
  if (pic_have_apic())
    apic_enable(irq);
  else
    i8259_enable(irq);
}

void
pic_disable(uint32_t irq)
{
  if (pic_have_apic())
    apic_disable(irq);
  else
    i8259_disable(irq);
}

void
pic_acknowledge(uint32_t irq)
{
  if (pic_have_apic())
    apic_acknowledge(irq);
  else
    i8259_acknowledge(irq);
}

bool
pic_isPending(uint32_t irq)
{
  if (pic_have_apic())
    return apic_isPending(irq);
  else
    return i8259_isPending(irq);
}
