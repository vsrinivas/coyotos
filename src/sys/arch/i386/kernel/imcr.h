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
 * @brief Interrupt Mode Control Register.
 *
 * The now-obsolete Intel Multiprocessor Specification introduces an
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

extern void ioapic_init();
