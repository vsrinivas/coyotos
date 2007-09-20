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

#define IO_8259_ICU1 0x20
#define IO_8259_ICU1_IMR 0x21
#define IO_8259_ICU2 0xA0
#define IO_8259_ICU2_IMR 0xA1

/** @brief Initialize the PC motherboard legacy ISA peripheral
 *  interrupt controllers.
 */
extern void i8259_init();

/** @brief Shut down the 8259 PIC so that IOAPIC can take over. */
extern void i8259_shutdown();

