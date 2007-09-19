#ifndef HAL_CONSOLE_H
#define HAL_CONSOLE_H
/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Console output mechanism.
 *
 * These functions are expected to be implemented by BSP-specific code.
 */

/** @brief Initialize the BSP console logic. */
extern void console_init();

/** @brief Append a character to the system console, if one exists */
extern void console_putc(char c);

/** @brief Stop using the console subsystem.
 *
 * Advises the console subsystem that we are about to start the
 * first user mode instruction, and that further attempts to put
 * output on the console should be ignored.
 */
extern void console_detach();

#endif /* HAL_CONSOLE_H */
