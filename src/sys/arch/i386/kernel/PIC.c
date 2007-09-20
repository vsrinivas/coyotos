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
#include "8259.h"
#include "ioapic.h"
#include "lapic.h"
#include "cpu.h"
#include "acpi.h"

#define DEBUG_IOAPIC if (0)

bool use_ioapic = false;

/** @brief Physical address of local APIC 
 *
 * This is the physical memory address where each CPU can access its
 * own local APIC. Value will be zero if there is no local APIC.
 */
kpa_t lapic_pa;

/** @brief Virtual address of local APIC 
 *
 * This is the virtual memory address where each CPU can access its
 * own local APIC. Value will be zero if there is no local APIC.
 */
kva_t lapic_va;

/****************************************************************
 * GENERIC WRAPPERS
 ****************************************************************/

void
pic_no_op(VectorInfo *vector)
{
}

void
pic_init()
{
  bool have_ioapic = acpi_probe_apics();

  if (use_ioapic && have_ioapic) {
    ioapic_init();

    printf("APIC, ");
  }
  else {
    i8259_init();
    printf("PIC, ");
  }

  if (use_ioapic && lapic_pa) {
    lapic_init();
    printf("LAPIC, ");
  }
}

void
pic_shutdown()
{
  (void) locally_disable_interrupts();

  if (use_ioapic)
    ioapic_shutdown();
}
