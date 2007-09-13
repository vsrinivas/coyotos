#ifndef __COYOTOS_CAPABILITY_STACK_H__
#define __COYOTOS_CAPABILITY_STACK_H__

/*
 * Copyright (C) 2007, The EROS Group, LLC
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
 * @brief interface definition for capability stack operations. */

#include <coyotos/coytypes.h>
#include <inttypes.h>

/** @brief Is there space for @p n caps on the stack? */
bool capability_canPush(uint32_t n);

/** @brief Push one cap on top of the stack */
bool capability_push(caploc_t cap);

/** @brief Pop the top of the stack. */
void capability_pop(caploc_t cap);

#endif /* __COYOTOS_CAPABILITY_STACK_H__ */
