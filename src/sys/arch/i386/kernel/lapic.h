#ifndef __I686_LAPIC_H__
#define __I686_LAPIC_H__
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

#include <stdbool.h>
#include <hal/kerntypes.h>
#include <kerninc/vector.h>

/** @brief Physical address of local APIC 
 *
 * This is the physical memory address where each CPU can access its
 * own local APIC. Value will be zero if there is no local APIC.
 *
 * Defined in PIC.c
 */
extern kpa_t lapic_pa;

/** @brief Virtual address of local APIC 
 *
 * This is the virtual memory address where each CPU can access its
 * own local APIC. Value will be zero if there is no local APIC.
 *
 * Defined in PIC.c
 */
extern kva_t lapic_va;



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
lapic_read_register(uint32_t reg)
{
  volatile uint32_t *va_reg = (uint32_t *) (lapic_va + reg);
  uint32_t val = *va_reg;
  return val;
}

static inline void
lapic_write_register(uint32_t reg, uint32_t val)
{
  volatile uint32_t *va_reg = (uint32_t *) (lapic_va + reg);
  *va_reg = val;

  /* Xeon errata: follow up with read from ID register, forcing above
     write to have observable effect: */
  (void) lapic_read_register(LAPIC_ID);
}

static inline void lapic_eoi()
{
  /* EOI is performed by writing an arbitrary value to the lapic EOI
   * register. */
  lapic_write_register(LAPIC_EOI, 0x1);
}

#endif /* __I686_LAPIC_H__ */
