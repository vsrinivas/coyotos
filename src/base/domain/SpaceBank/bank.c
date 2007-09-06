/*
 * Copyright (C) 2007, The EROS Group, LLC
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
 * @brief SpaceBank bank manipulation functions
 */

#include "SpaceBank.h"
#include <obstore/CoyImgHdr.h>

#define MAX_EXTENTS 20

Extent extents[MAX_EXTENTS];
size_t nExtents = 0;

Extent *extentByType[coyotos_Range_obType_otNUM_TYPES];

Object *freelistByType[coyotos_Range_obType_otNUM_TYPES];

Bank *primeBank;

oid_t prealloc_base[coyotos_Range_obType_otNUM_TYPES];

enum reserveType {
  RSV_FREE = 0,
  RSV_ALLOC = 1
};

static inline Object *
bank_getEndpoint(Bank *bank)
{
  Extent *ext = bank->extent;
  return &ext->array[bank - ext->bankArray];
}

static inline bool
bank_reserveSpace(Bank *bank, coyotos_Range_obType ty)
{
  Bank *cur;

  for (cur = bank; cur != 0; cur = cur->parent) {
    if (cur->usage[ty] >= cur->limits[ty])
      break;
    cur->usage[ty]++;
  }

  if (cur == 0)
    return true;

  /* failed; undo changes */
  while (bank != cur) {
    assert(bank->usage[ty] > 0);

    bank->usage[ty]--;
    bank = bank->parent;
  }
  return false;
}

static inline void
bank_unreserveSpace(Bank *bank, coyotos_Range_obType ty)
{
  Bank *cur;

  for (cur = bank; cur != 0; cur = cur->parent) {
    assert(cur->usage[ty] > 0);
    cur->usage[ty]--;
  }
}

static inline void
object_setAllocatedBank(Object *object, Bank *bank)
{
  coyotos_Range_obType ty = object->extent->obType;

  Object **listHead = (bank != 0)? &bank->oList : &freelistByType[ty];

  if (object->next != NULL) {
    Object **oldHead = (object->bank != 0)? &object->bank->oList :
      &freelistByType[ty];

    object->next->prev = object->prev;
    object->prev->next = object->next;
    if (*oldHead == object)
      *oldHead = object->next;
  }

  if (*listHead == 0) {
    object->next = object;
    object->prev = object;
    *listHead = object;
  } else {
    Object *head = *listHead;
    object->next = head;
    object->prev = head->prev;
    head->prev->next = object;
    head->prev = object;
  }
  object->bank = bank;
}

void
object_getCap(Object *obj, cap_t out)
{
  coyotos_Range_obType type = object_getType(obj);
  oid_t oid = object_getOid(obj);

  MUST_SUCCEED(coyotos_Range_getCap(CR_RANGE, oid, type, out, IDL_E));
}

/** @brief Free an object and tell the kernel to rescind it */
void
object_rescindAndFree(Object *obj)
{
  assert(obj->bank);
  oid_t oid = object_getOid(obj);
  coyotos_Range_obType ty = object_getType(obj);

  MUST_SUCCEED(coyotos_Range_getCap(CR_RANGE, oid, ty, CR_TMP1, IDL_E));
  MUST_SUCCEED(coyotos_Range_rescind(CR_RANGE, CR_TMP1, IDL_E));

  // rescind obj and free it.
  bank_unreserveSpace(obj->bank, ty);
  object_setAllocatedBank(obj, NULL);
}

static inline void
require_object(coyotos_Range_obType ty, cap_t out)
{
  Extent *ext = extentByType[ty];

  assert(ext->bootstrapped >= 0);
  assert(ext->count > (ext->preallocated + ext->bootstrapped));

  oid_t oid = ext->baseOID + ext->preallocated + ext->bootstrapped;
  ext->bootstrapped++;

  MUST_SUCCEED(coyotos_Range_getCap(CR_RANGE, oid, ty, out, IDL_E));
}

void
require_Page(cap_t out)
{
  require_object(coyotos_Range_obType_otPage, out);
}

void
require_GPT(cap_t out)
{
  require_object(coyotos_Range_obType_otGPT, out);
}

void
get_pagecap(cap_t out, oid_t oid)
{
  MUST_SUCCEED(coyotos_Range_getCap(CR_RANGE,
				    oid,
				    coyotos_Range_obType_otPage,
				    out,
				    IDL_E));
}

bool
bank_setLimits(Bank *bank, const coyotos_SpaceBank_limits *newLims)
{
  size_t idx;
  for (idx = 0; idx < coyotos_Range_obType_otNUM_TYPES; idx++)
    if (bank->usage[idx] > newLims->byType[idx])
      return false;

  for (idx = 0; idx < coyotos_Range_obType_otNUM_TYPES; idx++)
    bank->limits[idx] = newLims->byType[idx];

  return true;
}

void
bank_getUsage(Bank *bank, coyotos_SpaceBank_limits *out)
{
  size_t idx;
  for (idx = 0; idx < coyotos_Range_obType_otNUM_TYPES; idx++)
    out->byType[idx] = bank->usage[idx];
}

void
bank_getLimits(Bank *bank, coyotos_SpaceBank_limits *out)
{
  size_t idx;
  for (idx = 0; idx < coyotos_Range_obType_otNUM_TYPES; idx++)
    out->byType[idx] = bank->limits[idx];
}

void
bank_getEffLimits(Bank *bank, coyotos_SpaceBank_limits *out)
{
  Bank *cur;
  size_t idx;

  // Fill the out array with the number of available objects in bank.
  for (idx = 0; idx < coyotos_Range_obType_otNUM_TYPES; idx++)
    out->byType[idx] = -1ULL; // unlimited.

  // Find the most constrained values by walking up the parent chain.
  for (cur = bank->parent; cur != 0; cur = cur->parent) {
    for (idx = 0; idx < coyotos_Range_obType_otNUM_TYPES; idx++) {
      unsigned long long avail = cur->limits[idx] - cur->usage[idx];
      if (avail < out->byType[idx])
	out->byType[idx] = avail;
    }
  }

  // Add in the child bank's usage to get the effective limit, as opposed
  // to the # of objects available.
  for (idx = 0; idx < coyotos_Range_obType_otNUM_TYPES; idx++)
    out->byType[idx] += bank->usage[idx];
}

bool
bank_create(Bank *parent, cap_t out)
{
  Object *endpt = bank_alloc(parent, coyotos_Range_obType_otEndpoint, out);
  if (endpt == 0)
    return false;

  Extent *ext = endpt->extent;
  Bank *bank = &ext->bankArray[endpt - ext->array];

  assert(bank->parent = 0 && bank != primeBank);

  // link it in to the bank hierarchy.
  bank->parent = parent;
  bank->nextSibling = parent->firstChild;
  parent->firstChild = bank;

  // set up the endpoint, and make the initial Entry cap.
  MUST_SUCCEED(coyotos_Endpoint_setRecipient(out, CR_SELF, IDL_E));
  // we offset the endpointID by one, since 0 is our reply endpoint.
  MUST_SUCCEED(coyotos_Endpoint_setEndpointID(out,
					      object_getOid(endpt) + 1,
					      IDL_E));
  // a protected payload of 0 gives all permissions
  MUST_SUCCEED(coyotos_Endpoint_makeEntryCap(out, 0, out, IDL_E));

  return true;
}

void
bank_destroy(Bank *bank, bool destroyObjects)
{
  Bank *parent = bank->parent;

  // don't destroy the PrimeBank
  assert(bank->parent != 0);

  while (bank != parent) {
    /* go down to a leaf child */
    while (bank->firstChild)
      bank = bank->firstChild;

    // unlink the bank from its siblings and parent bank.
    Bank **ptr;
    for (ptr = &bank->parent->firstChild;
	 *ptr != bank;
	 ptr = &(*ptr)->nextSibling)
      assert((*ptr) != 0);

    *ptr = bank->nextSibling;
    bank->nextSibling = 0;

    // Rescind the bank's endpoint, to make it so no one can use it.
    Object *endpoint = bank_getEndpoint(bank);
    object_rescindAndFree(endpoint);

    if (destroyObjects) {
      // Rescind and free all objects in the bank
      Object *obj;
      while ((obj = bank->oList) != 0)
	object_rescindAndFree(obj);

    } else {
      // move the objects in the bank into the parent
      // <b>of the destroyed bank</b>.

      // Note that all allocations have already been accounted for in the
      // parent, so no bookkeeping needs to happen.
      Object *obj;
      while ((obj = bank->oList) != 0)
	object_setAllocatedBank(obj, parent);
    }

    Bank *next = bank->parent;

    // clean up the bank
    bank->parent = 0;
    assert(bank->oList == 0);

    coyotos_Range_obType type;
    for (type = 0; type < coyotos_Range_obType_otNUM_TYPES; type++) {
      bank->limits[type] = -1ULL;
      bank->usage[type] = 0;
    }

    // set up for next loop
    bank = next;
  }
}

void
load_initial_extent(coyotos_Range_obType type)
{
  assert(nExtents < MAX_EXTENTS);
  Extent *ext = &extents[nExtents++];

  oid_t base = 0;
  oid_t bound = 0;

  MUST_SUCCEED(coyotos_Range_nextBackedSubrange(CR_RANGE, 0, type,
						&base, &bound, IDL_E));

  assert((size_t)(bound - base) == (bound - base));

  ext->baseOID = base;
  ext->count = bound - base;
  ext->obType = type;
  ext->nextExtent = extentByType[type];
  extentByType[type] = ext;
}

/** @brief Allocate the Object and Bank structures for the existing extents. */
void
allocate_objects_and_banks(void)
{
  size_t idx;
  size_t objidx;

  for (idx = 0; idx < nExtents; idx++) {
    Extent *ext = &extents[idx];
    Object *array = allocate_bytes(sizeof (*ext->array) * ext->count);
    assert(array != 0);
    ext->array = array;
    for (objidx = 0; objidx < ext->count; objidx++) {
      array[objidx].extent = ext;
      // add to freelist
      object_setAllocatedBank(&array[objidx], NULL);
    }
    if (ext->obType != coyotos_Range_obType_otEndpoint)
      ext->bankArray = NULL;
    else {
      Bank *bankArray = allocate_bytes(sizeof (*ext->bankArray) * ext->count);
      assert(bankArray != 0);
      ext->bankArray = bankArray;
      for (objidx = 0; objidx < ext->count; objidx++) {
	bankArray[objidx].extent = ext;
	int ty;
	for (ty = 0; ty < coyotos_Range_obType_otNUM_TYPES; ty++)
	  bankArray[objidx].limits[ty] = -1ULL;  /* unlimited */
      }
    }
  }
}

/** @brief locate Object structure for @p type and @p oid */
static Object *
lookup_object(coyotos_Range_obType type, oid_t oid)
{
  assert(type < coyotos_Range_obType_otNUM_TYPES);

  Extent *ext;

  for (ext = extentByType[type]; ext != NULL; ext = ext->nextExtent) {
    assert(ext->obType == type);
    if (oid >= ext->baseOID && (oid - ext->baseOID) < ext->count)
      return &ext->array[oid - ext->baseOID];
  }
  return (0);
}

/** @brief Find the Bank structure for oid @p oid */
static Bank *
lookup_bank(oid_t oid)
{
  coyotos_Range_obType type = coyotos_Range_obType_otEndpoint;

  Extent *ext;

  for (ext = extentByType[type]; ext != NULL; ext = ext->nextExtent) {
    assert(ext->obType == type);
    assert(ext->bankArray != 0);
    if (oid >= ext->baseOID && (oid - ext->baseOID) < ext->count)
      return (&ext->bankArray[oid - ext->baseOID]);
  }
  return (0);
}

Bank *
bank_fromEPID(oid_t oid)
{
  // de-bias the OID
  Bank *bank = lookup_bank(oid - 1);
  // if bank does not exist or is not allocated, fail
  if (bank == 0 || (bank->parent == 0 && bank != primeBank))
    return 0;

  return bank;
}
/** @brief Setup the initial preallocation of the initial extent, as well
 * as the bootstrapping allocation
 */
static void
setup_prealloc(coyotos_Range_obType type, size_t count)
{
  Extent *ext = extentByType[type];
  assert(ext->baseOID == 0);
  assert(ext->count > count);

  ext->preallocated = count;
  ext->bootstrapped = 0;
}

Object *
bank_do_alloc(Bank *bank, coyotos_Range_obType type, cap_t out)
{
  bool result = bank_reserveSpace(bank, type);

  if (!result)
    return 0;

  Object *obj = freelistByType[type];
  object_setAllocatedBank(obj, bank);
  return obj;
}

Object *
bank_alloc(Bank *bank, coyotos_Range_obType type, cap_t out)
{
  Object *obj = bank_do_alloc(bank, type, out);
  if (obj) {
    oid_t oid = object_getOid(obj);
    MUST_SUCCEED(coyotos_Range_getCap(CR_RANGE, oid, type, out, IDL_E));
  }
  return obj;
}

Object *
bank_alloc_proc(Bank *bank, cap_t brand, cap_t out)
{
  Object *obj = bank_do_alloc(bank, coyotos_Range_obType_otProcess, out);
  if (obj) {
    oid_t oid = object_getOid(obj);
    MUST_SUCCEED(coyotos_Range_getProcess(CR_RANGE, oid, brand, out, IDL_E));
  }
  return obj;
}

void
bank_getEntry(Bank *bank, coyotos_SpaceBank_restrictions restr, cap_t out)
{
  Object *endpt = bank_getEndpoint(bank);
  MUST_SUCCEED(coyotos_Range_getCap(CR_RANGE,
				    object_getOid(endpt),
				    object_getType(endpt),
				    out,
				    IDL_E));

  MUST_SUCCEED(coyotos_Endpoint_makeEntryCap(out, restr, out, IDL_E));

}

void
object_unalloc(Object *obj)
{
  Bank *bank = obj->bank;
  assert(bank);

  bank_unreserveSpace(bank, object_getType(obj));
  object_setAllocatedBank(obj, NULL);
}

Object *
object_identify(cap_t cap)
{
  coyotos_Range_obType type = coyotos_Range_obType_otNUM_TYPES;
  oid_t oid;

  if (!coyotos_Range_identify(CR_RANGE, cap, &type, &oid, IDL_E))
    return 0;
  if (type == coyotos_Range_obType_otInvalid)
    return 0;

  assert(type >= 0 && type < coyotos_Range_obType_otNUM_TYPES);

  return lookup_object(type, oid);
}

/** @brief Set up the banks from the Bank array supplied from mkimage,
 * Located at @p banks and @p count
 *
 * Also sets up their Endpoints correctly.
 */
void
setup_banks(CoyImgBank *banks, size_t count)
{
  size_t idx;

  for (idx = 0; idx < count; idx++) {
    oid_t oid = banks[idx].oid;
    bankid_t parentId = banks[idx].parent;

    /*
     * The parent must have already been set up.  This guarantees that the
     * bank tree is well-formed. (i.e. a tree rooted at primeBank)
     */
    if (parentId > 0)
      assert3(parentId, <, (bankid_t)idx);

    oid_t parentOid = banks[parentId].oid;

    Bank *bank = lookup_bank(oid);
    Bank *parent = lookup_bank(parentOid);

    // they both must exist
    assert3p(bank, !=, 0);
    assert3p(parent, !=, 0);

    // link the bank into its parent chain
    if (bank == parent) {
      assert(primeBank == 0);
      primeBank = bank;
      bank->parent = 0;
    } else {
      bank->parent = parent;
      bank->nextSibling = parent->firstChild;
      parent->firstChild = bank;
    }

    // Get the endpoint, point it at us, and set its endpoint ID appropriately.
    MUST_SUCCEED(coyotos_Range_getCap(CR_RANGE,
				      oid,
				      coyotos_Range_obType_otEndpoint,
				      CR_TMP1,
				      IDL_E));

    MUST_SUCCEED(coyotos_Endpoint_setRecipient(CR_TMP1, CR_SELF, IDL_E));
    // we offset the endpoint ID by 1, since 0 is our reply endpoint.
    MUST_SUCCEED(coyotos_Endpoint_setEndpointID(CR_TMP1, oid + 1, IDL_E));
  }
  // There must have been a primeBank somewhere.
  assert(primeBank != 0);

  // set up prime bank's limits to match the range counts.
  size_t ty;
  for (ty = 0; ty < coyotos_Range_obType_otNUM_TYPES; ty++) {
    primeBank->limits[ty] = 0;
    Extent *ext;
    for (ext = extentByType[ty]; ext != 0; ext = ext->nextExtent)
      primeBank->limits[ty] += ext->count;
  }
}

/** @brief Actually do the initial allocations described in the CoyImgAlloc
 * array @p alloc of size @p count, which was created by mkimage.
 */
static void
setup_allocations(CoyImgAlloc *alloc, size_t count)
{
  size_t idx;

  for (idx = 0; idx < count; idx++) {
    oid_t oid = alloc[idx].oid;
    bankid_t bankid = alloc[idx].bank;
    coyotos_Range_obType type = alloc[idx].fType;

    Bank *bank = lookup_bank(bankid);
    Object *obj = lookup_object(type, oid);

    bool result = bank_reserveSpace(bank, type);
    assert(result);

    object_setAllocatedBank(obj, bank);
  }
}

/** @brief Actually do the bootstrap allocations, allocating them to
 * the Prime Bank.
 */
static void
setup_bootstrap_allocations(void)
{
  size_t idx;

  for (idx = 0; idx < nExtents; idx++) {
    Extent *ext = &extents[idx];

    size_t objIdx;
    assert(ext->bootstrapped != -1ul);
    for (objIdx = 0; objIdx < ext->bootstrapped; objIdx++) {
      Object *obj = &ext->array[ext->preallocated + objIdx];

      assert(obj->bank == NULL);
      // treat it as allocated by the primeBank
      bool result = bank_reserveSpace(primeBank, object_getType(obj));
      assert(result);
      object_setAllocatedBank(obj, primeBank);
    }
    ext->bootstrapped = -1ul;
  }
}

void
initialize(void)
{
  coyotos_Range_obType type;

  alloc_init();

  for (type = 0; type < coyotos_Range_obType_otNUM_TYPES; type++)
    load_initial_extent(type);

  setup_prealloc(coyotos_Range_obType_otPage, image_header->nPage);
  setup_prealloc(coyotos_Range_obType_otCapPage, image_header->nCapPage);
  setup_prealloc(coyotos_Range_obType_otGPT, image_header->nGPT);
  setup_prealloc(coyotos_Range_obType_otEndpoint, image_header->nEndpoint);
  setup_prealloc(coyotos_Range_obType_otProcess, image_header->nProc);

  // Now that we've reserved the initially allocated objects, we can start
  // allocating storage and mapping tables.
  allocate_objects_and_banks();

  void *metadataMap = map_pages(image_header->bankVecOID,
				image_header->endVecOID);


  alloc_finish();

  // from now on, no allocate_bytes() or map_pages() may occur.

  setup_banks((CoyImgBank *)metadataMap, image_header->nBank);

  uintptr_t allocVecBase = (uintptr_t)metadataMap +
    COYOTOS_PAGE_SIZE * (image_header->allocVecOID - image_header->bankVecOID);

  setup_allocations((CoyImgAlloc *)allocVecBase, image_header->nAlloc);

  // Mark the pages and GPTs we allocated bootstrapping this operation as
  // allocated from the Prime Bank.
  setup_bootstrap_allocations();

  // at this point, our ranges, banks, and freelists are fully consistent.

  /// @bug at some point, we should rescind and free the metadata pages, and
  /// possibly the GPTs which point to them.
}

