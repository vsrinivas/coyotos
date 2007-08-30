#ifndef __KERNINC_ANNOTATIONS_H__
#define __KERNINC_ANNOTATIONS_H__
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
 *
 * @brief Macro definitions to "remove" various qualifier-style
 * annotationsthat are present for documentation purposes.
 *
 * Entries in this file should consist of
 *
 *   #define IDENT
 *
 * with no value defined. These may be ifdef'd so that other tools,
 * e.g. cqual, can see the qualifiers.
 *
 * This file is not explicitly included anywhere. It is included at
 * the compiler line by a "pre-include" directive.
 */

/** @brief Indicates that a declared procedure or variable is supplied
   by architecture-dependent code. */
#define __hal

#endif /* __KERNINC_ANNOTATIONS_H__ */
