/*
 * Copyright (C) 2007, The EROS Group, LLC.
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
 * @brief Object store interface from kernel perspective.
 */

#include <kerninc/ObStore.h>
#include <kerninc/ObjectHash.h>
#include <kerninc/Cache.h>
#include <kerninc/assert.h>

ObjectHeader*
obstore_require_object(ObType ty, oid_t oid, bool waitForRange, HoldInfo *hi)
{
  HoldInfo hashMutex = obhash_grabMutex(ty, oid);
  ObjectHeader *obHdr = obhash_lookup(ty, oid, false, hi);

  if (!obHdr) {
    /* check that the OID is within the allowed range */
    if (oid >= Cache.max_oid[ty]) {
      assert(!waitForRange);
      return 0;
    }

    obHdr = cache_alloc(ty);
    obHdr->ty = ty;
    obHdr->oid = oid;
    obHdr->allocCount = 0;
    obHdr->current = 1;

    cache_install_new_object(obHdr);
    obhash_insert(obHdr);
  }
  assert(obHdr);

  /* Fetch object from disk */

  mutex_release(hashMutex);

  return obHdr;
}

void
obstore_write_object_back(ObjectHeader *hdr)
{
  assert(false);
}
