#ifndef __I686_IOAPIC_H__
#define __I686_IOAPIC_H__

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
 * @brief IO APIC support.
 */

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


/** @brief Start the IOAPIC-based interrupt system.
 *
 * Shuts down the 80259 interrupt controller system and initializes
 * all registered IOAPICs.
 */
extern void ioapic_init(); 

/** @brief Shut down the IOAPIC system prior to soft reboot.
 *
 * @bug Not currently implemented in any meaningful sense.
 */
extern void ioapic_shutdown();

/** @brief Register an IOAPIC chip with the IOAPIC subsystem.
 *
 * All IOAPIC chips discovered should be registered before calling
 * ioapic_init().  Called by acpi_probe_apics().
 */
extern void ioapic_register(irq_t baseIRQ, kva_t va);

#endif /* __I686_IOAPIC_H__ */
