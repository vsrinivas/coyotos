#ifndef __KERNINC_OBSTORE_H__
#define __KERNINC_OBSTORE_H__
/*
 * Copyright (C) 2007, The EROS Group, LLC.
 *
 * This file is part of the EROS Operating System runtime library.
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
 * @brief Kernel interface to the object store layer. */

#include <kerninc/ObjectHeader.h>
#include <coyotos/coytypes.h>

#if 0
/** @brief Ask object store to load current version of object named by
 * (type,oid) into the kernel.
 *
 * Returned object will be locked. */
extern ObjectHeader *obstore_load_object(ObType ty, oid_t oid, bool willWait, HoldInfo *hi);
#endif

/** @brief Find the current version of the object named by (type,oid),
 * loading it if necessary.
 *
 * Returned object will be locked. */
extern ObjectHeader *obstore_require_object(ObType ty, oid_t oid, bool willWait, HoldInfo *hi);

/** @brief Write dirty object state back to object store. May be
 * either current or snapshot version. */
extern void obstore_write_object_back(ObjectHeader *);

#endif /* __KERNINC_OBSTORE_H__ */
