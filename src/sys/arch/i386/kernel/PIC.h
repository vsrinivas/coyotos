#ifndef __I686_PIC_H__
#define __I686_PIC_H__
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

/** @brief True if we should attempt to use local APIC.
 */
extern bool use_apic;

/** @brief True if implementation has a redundant 8259 legacy PIC that
 * must be disabled before the local APIC is enabled.
 *
 * Set conditionally in acpi.c:acpi_find_MADT()
 */
extern bool lapic_requires_8259_disable;

/** @brief Physical address of local APIC 
 *
 * This is the physical memory address where each CPU can access its
 * own local APIC. Value will be zero if there is no local APIC.
 */
extern kpa_t lapic_pa;

/** @brief Virtual address of local APIC 
 *
 * This is the virtual memory address where each CPU can access its
 * own local APIC. Value will be zero if there is no local APIC.
 */
extern kva_t lapic_va;

/** @brief Physical address of I/O APIC 
 *
 * This is the physical memory address of the I/O APIC.
 * Value will be zero if there is no I/O APIC.
 *
 * @bug There is a model issue here, because systems can have more
 * than one I/O APIC. Current implementation does not handle this.
 */
extern kpa_t ioapic_pa;

/** @brief Virtual address of I/O APIC 
 *
 * This is the virtual memory address of the I/O APIC.
 * Value will be zero if there is no I/O APIC.
 *
 * @bug There is a model issue here, because systems can have more
 * than one I/O APIC. Current implementation does not handle this.
 */
extern kva_t ioapic_va;

/** @brief Return true if the system has a local APIC that is actually
 * in use by us.
 *
 * This is mainly used internal to the PIC implementation. It is
 * exported so that we can make decisions about which interval timer
 * to initialize.
 */
bool pic_have_apic();

/** @brief Initialize the preferred peripheral interrupt controller. */
void pic_init();

void pic_shutdown();

/** @brief Return true IFF the interrupt line corresponding to @p vector
 * actually has a pending interrupt.
 *
 * This permits detection and suppression of spurious hardware
 * interrupts.
 */
bool pic_isPending(uint32_t irq);

/** @brief Enable the interrupt line corresponding to @p vector */
void pic_enable(uint32_t irq);

/** @brief Disable the interrupt line corresponding to @p vector */
void pic_disable(uint32_t irq);

/** @brief Acknowledge to the PIC that an interrupt on the interrupt line
 * corresponding to @p vector has been received by the OS.
 *
 * This should typically be done only after disabling the interrupt at
 * the PIC.
 */
void pic_acknowledge(uint32_t irq);

#endif /* __I686_PIC_H__ */
