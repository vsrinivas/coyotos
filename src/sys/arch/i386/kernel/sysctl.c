/*
 * Copyright (C) 2007, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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
 * @brief System control
 */

#include <stddef.h>
#include <kerninc/printf.h>
#include "PIC.h"
#include "IRQ.h"

void 
sysctl_reboot()
{
  printf("NOTICE: Hard reboot requested.\n");
  pic_shutdown();

  irq_DoTripleFault();
}

void 
sysctl_powerdown()
{
  printf("NOTICE: Powerdown requested.\n");

  pic_shutdown();

  /** @bug Proper powerdown requires switching to real mode, which we
   * do not currently have code to do, and I don't have time to deal
   * with that right now.
   */
  sysctl_halt();
}
