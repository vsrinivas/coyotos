#ifndef __KERNINC_OBJECTHASH_H__
#define __KERNINC_OBJECTHASH_H__
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

#include <kerninc/ObjectHeader.h>

/** @brief Grab the appropriate bucket mutex for the object type and
 * OID. */
HoldInfo
obhash_grabMutex(ObType ty, oid_t oid);


/** @brief Insert an object into the object hash table.
 *
 * Insert @p ob into the object hash.  <tt>ob</tt>'s ty and oid fields
 * must be set up properly.  
 *
 * Operational Preconditions
 *
 * - hdr must not be in the hash table already.
 * - hdr must have a valid frame type, oid.
 * - hdr must have (snapshot|current) != 0.
 *
 * Sanity Preconditions:
 *
 * - There may be at *most* one ObjectHeader in the hash table with a
 *   matching type and oid to hdr. If there is one, it must be marked
 *   both snapshot and current, and the incoming hdr must be marked
 *   current and not snapshot. 
 * - hdr must have an invalid otIndex.
 * - Caller must be exclusive holder of the object being inserted.
 *
 * Postconditions:
 * - obhash_lookup(hdr->type, hdr->oid, hdr->snapshot) == hdr 
 *
 * Note that the snapshot mechanism relies on the invariant that the
 * most recently inserted object of a given (type, oid) that is marked
 * "current" (therefore the first found in the hash) is the actual
 * current object. This resolves a race in the conversion of current
 * to snapshot, wherein we must not drop the old "current" marker
 * before a new one is present.
 */
extern void obhash_insert(ObjectHeader *ob);

/**
 * Returns a locked object of type @p ty and OID @p oid in the hash
 * table if found, returning the current or snapshot version as
 * indicated by @p wantSnapshot.
 *
 * If @p wantSnapshot is set, the @em first matching object marked
 * "snapshot" will be returned, else the @em first matching object
 * marked "current" will be returned.
 *
 * The object is returned locked.  If @p out is non-NULL, *@p out will
 * be filled in with the necessary HoldInfo structure.
 */
extern ObjectHeader *
obhash_lookup(ObType ty, oid_t oid, bool wantSnapshot, HoldInfo *hi);
/**
 * Remove @p ob from the Object Hash.
 *
 * The caller must already hold the per-object lock on @p ob.
 */
extern void obhash_remove(ObjectHeader *ob);

#define obhash_insert_obj(o) obhash_insert((ObjectHeader *)o)

#define obhash_remove_obj(o) obhash_remove((ObjectHeader *)o)

#endif /* __KERNINC_OBJECTHASH_H__ */
