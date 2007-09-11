#ifndef I386_HAL_MACHINE_H
#define I386_HAL_MACHINE_H
/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Miscellaneous machine-specific functions.
 */

#include <kerninc/ccs.h>

/** @brief Halt the processor without rebooting.
 *
 * Implementation is architecture specific.
 */
extern void sysctl_halt(void) NORETURN;

/** @brief Power down the system if possible.
 *
 * Implementation is architecture specific.
 */
extern void sysctl_powerdown(void) NORETURN;

/** @brief Perform an orderly shutdown and reboot the system if possible.
 *
 * Implementation is architecture specific.
 */
extern void sysctl_reboot(void) NORETURN;


#endif /* I386_HAL_MACHINE_H */
