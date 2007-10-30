#ifndef __COYOTOS_CAPTEMP_H__
#define __COYOTOS_CAPTEMP_H__

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
 * @brief interface definition for capability temporaries for library use
 **/

#include <coyotos/coytypes.h>
#include <inttypes.h>

/** @brief Allocate a temporary capability.
 *
 * Returns a unused caploc_t with undefined current contents, for use as
 * a temporary.  Must be balanced with a call to captemp_release().
 *
 * Capability temporaries must be used with a strict stack discipline.
 *
 * @bug if we run out, some kind of Process exception?
 */
caploc_t captemp_alloc();

/** @brief Release a temporary capability, and any outstanding 
 * temporaries allocated after it.
 *
 * Releases @p cap, which must be the most recent temporary have been
 * returned from a call to captemp_alloc(), and any non-released
 * temporaries allocated after @p cap was allocated.
 *
 * @bug If improper releases are done, do we throw an exception?
 */
void captemp_release(caploc_t cap);

#endif /* __COYOTOS_CAPTEMP_H__ */
