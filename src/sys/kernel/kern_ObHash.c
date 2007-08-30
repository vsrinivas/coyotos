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
 * @brief Object Hash.
 *
 * Objects named by a type and an OID are universally accessed via this
 * hash table.
 *
 * kern_ObHash.c handles the management of this hash table.
 */

#include <coyotos/coytypes.h>
#include <kerninc/assert.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/mutex.h>
#include <kerninc/malloc.h>

/* this should really be dynamically sized. */

#define OBHASH_BITS 12
#define OBHASH_SIZE (1u << OBHASH_BITS)

struct obhash_bucket {
  mutex_t lock;
  ObjectHeader *head;
};

static struct obhash_bucket obhash[OBHASH_SIZE];

static inline struct obhash_bucket *
obhash_hash(ObType ty, oid_t oid)
{
  size_t hash = (ty << (OBHASH_BITS - 3)) ^ oid ^ (oid >> OBHASH_BITS);

  return &obhash[hash & (OBHASH_SIZE - 1)];
}

HoldInfo
obhash_grabMutex(ObType ty, oid_t oid)
{
  struct obhash_bucket *bucket = obhash_hash(ty, oid);
  return mutex_grab(&bucket->lock);
}

void
obhash_insert(ObjectHeader *ob)
{
  struct obhash_bucket *bucket = obhash_hash(ob->ty, ob->oid);
  HoldInfo hi = mutex_grab(&bucket->lock);

  ob->next = bucket->head;
  bucket->head = ob;
  mutex_release(hi);
}

ObjectHeader *
obhash_lookup(ObType ty, oid_t oid, bool wantSnapshot, HoldInfo *out)
{
  struct obhash_bucket *bucket = obhash_hash(ty, oid);
  ObjectHeader *cur;

  HoldInfo hi = mutex_grab(&bucket->lock);

  for (cur = bucket->head; cur != NULL; cur = cur->next) {
    if (cur->ty == ty && cur->oid == oid) {
      assert(cur->snapshot || cur->current);
      if (wantSnapshot && !cur->snapshot)
	continue;
      if (!wantSnapshot && !cur->current)
	continue;

      // Acquire object lock
      HoldInfo hi_cur = mutex_grab(&cur->lock);

      // OUT holdinfo may not be passed if caller plans to exploit
      // gang-release. It may turn out that we never need to use it.
      if (out)
	*out = hi_cur;
      break;
    }
  }

  mutex_release(hi);
  return (cur);
}

void
obhash_remove(ObjectHeader *ob)
{
  struct obhash_bucket *bucket = obhash_hash(ob->ty, ob->oid);
  ObjectHeader **cur;

  HoldInfo hi = mutex_grab(&bucket->lock);

  for (cur = &bucket->head; *cur != NULL; cur = &(*cur)->next) {
    if (*cur == ob) {
      *cur = ob->next;
      mutex_release(hi);
      return;
    }
  }
      
  assert(false && "object not in bucket");
}
