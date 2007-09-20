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
#include <kerninc/vector.h>

/** @brief True if we should attempt to use local APIC.
 */
extern bool use_apic;

/** @brief Initialize the preferred peripheral interrupt controller. */
void pic_init();

void pic_shutdown();

void pic_no_op(struct IrqController *chip, irq_t irq);

#endif /* __I686_PIC_H__ */
