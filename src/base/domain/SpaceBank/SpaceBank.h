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
 * @brief SpaceBank data structures
 */

#ifndef SPACEBANK_H__
#define SPACEBANK_H__

#include <inttypes.h>
#include <stddef.h>
#include <coyotos/coytypes.h>
#include <coyotos/syscall.h>
#include <coyotos/runtime.h>
#include <idl/coyotos/Memory.h>
#include <idl/coyotos/GPT.h>
#include <idl/coyotos/Endpoint.h>
#include <idl/coyotos/Process.h>
#include <idl/coyotos/Range.h>
#include <idl/coyotos/SpaceBank.h>

#include "BUILD/coyotos.SpaceBank.h"

/* Turn constants defined in .mki file into usable capability registers */

#define CR_RANGE	CR_APP(coyotos_SpaceBank_APP_RANGE)
#define CR_INITGPT	CR_APP(coyotos_SpaceBank_APP_INITGPT)
#define CR_ADDRSPACE	CR_APP(coyotos_SpaceBank_APP_ADDRSPACE)
#define CR_TMP1	CR_APP(coyotos_SpaceBank_APP_TMP1)
#define CR_TMP2	CR_APP(coyotos_SpaceBank_APP_TMP2)
#define CR_TMP3	CR_APP(coyotos_SpaceBank_APP_TMP3)
#define CR_TMPPAGE	CR_APP(coyotos_SpaceBank_APP_TMPPAGE)

/* Define our basic datastructures. */

struct Bank;
typedef struct Bank Bank;
struct Extent;
typedef struct Extent Extent;
struct Object;
typedef struct Object Object;

/**
 * @brief Structure describing a single object, either allocated or free.
 *
 * For an object <tt>obj</tt>, its type is <tt>obj->extent->obType</tt>. 
 * Its OID is <tt>obj->extent->baseOID + (obj - obj->extent->array)</tt>
 */
struct Object {
  Object *next; /**< @brief next Object in freelist or bank's oList  */
  Object *prev; /**< @brief previous Object in freelist or bank's oList  */
  Bank *bank;   /**< @brief Bank object is allocated from, or NULL if free */
  Extent *extent; /**< @brief Extent containing, this object */
};

/** @brief Structure describing an extent of OIDs.
 *
 * Contains an array of Object structures;  to compute the OID of an object,
 * add its array index to baseOID.
 */
struct Extent {
  coyotos_Range_oid_t baseOID;
  size_t count;

  size_t preallocated; /**< @brief count of image-allocated objects */
  size_t bootstrapped; /**< @brief count of bootstrap-allocated objects */

  Object *array; /**< @brief array of @p count Object structures */
  /** @brief array of @p count Bank structures, for Endpoint extents.  
   *
   * NULL for non-Endpoint extents.
   */
  Bank *bankArray;
  Extent *nextExtent; /**< @brief next Extent of this type */
  coyotos_Range_obType obType; /**< @brief type of this Extent */
};

struct Bank {
  /** @brief The extent this Bank's endpoint was allocated from. */
  Extent *extent; 

  Bank *parent; /**< @brief parent of this Bank */
  Bank *nextSibling; /**< @brief Next sibling of this Bank */
  Bank *firstChild; /**< @brief First child of this Bank */
  
  /**< @brief head of the doubly-linked list of object allocated from 
   * this Bank. 
   */
  Object *oList;

  uint64_t limits[coyotos_Range_obType_otNUM_TYPES];
  uint64_t usage[coyotos_Range_obType_otNUM_TYPES];
};

extern IDL_Environment _IDL_E;
#define IDL_E (&_IDL_E)

static inline bool
check_obType(coyotos_Range_obType ty)
{
  return (ty == coyotos_Range_obType_otInvalid || 
	  ty < coyotos_Range_obType_otNUM_TYPES);
}

/** @brief get the obType of an object */
static inline coyotos_Range_obType
object_getType(Object *obj)
{
  return (obj->extent->obType);
}

/** @brief get the OID for an object */
static inline coyotos_Range_oid_t 
object_getOid(Object *obj)
{
  Extent *e = obj->extent;
  return (e->baseOID + (obj - e->array));
}

#define MUST_SUCCEED(x) \
  do { bool __r = (x); assert(__r); } while(0)

/** @brief A cannot-fail bootstrap allocation of a GPT */
void require_GPT(cap_t out);

/** @brief A cannot-fail bootstrap allocation of a Page */
void require_Page(cap_t out);

extern const struct CoyImgHdr *image_header;

/** @brief Initialize the system.
 *
 * Does a pile of setup, at the end of which we are ready to answer requests.
 */
void initialize(void);

/** @brief initialize the alloc subsystem and address space.
 *
 * After this completes, @p image_header is valid and available for use
 */
void alloc_init(void);

/** @brief finalize the alloc subsystem.
 *
 * After this completes, allocate_bytes() and map_pages may no longer be
 * called;  the memory map is fixed.
 */
void alloc_finish(void);

/** @brief allocates @p bytes of zero-filled storage aligned appropriately */
void *allocate_bytes(size_t bytes_arg);

/** @brief map Page oids from @p start to @p end */
void *map_pages(oid_t start, oid_t end);

/** @brief Get a capability to a particular page; cannot fail */
void get_pagecap(cap_t out, oid_t oid);

/** @brief Allocate an object of type @p ty from @p bank, placing the
 * cap in @p out.  Returns the Object structure for the new object, or
 * null if the limit was reached..
 */
Object *bank_alloc(Bank *bank, coyotos_Range_obType ty, cap_t out);

/** @brief Allocate a Process from @p bank, placing @p brand in its
 * brand slot, and placing the cap in @p out.  Returns the Object
 * structure for the new object, or NULL if the limit was reached.
 */
Object *bank_alloc_proc(Bank *bank, cap_t brand, cap_t out);

/** @brief lookup a Bank structure for an endpoint ID */
Bank *bank_fromEPID(oid_t oid);

/** @brief get an entry cap to @p bank with restrictions @p restr, and place
 * it in @p out. */
void bank_getEntry(Bank *bank, 
		   coyotos_SpaceBank_restrictions restr, 
		   cap_t out);

/** @brief Create a new child bank.  Fails if limits preclude the creation.
 */
bool bank_create(Bank *parent, cap_t out);

/** @brief Destroy the bank @p bank, and either:
 *   <ol>
 *     <li>If @p destroyObjects is true, destroy all outstanding objects 
 *        allocated from the bank.</li>
 *     <li>If @p destroyObjects is false, re-assign all outstanding objects
 *        to the parent bank.</li>
 *   </ol>
 */
void bank_destroy(Bank *bank, bool destroyObjects);

/** @brief Attempt to set the limits on a bank.  If the outstanding allocations
 * are larger than the existing limits, this will fail.
 */
bool bank_setLimits(Bank *bank, const coyotos_SpaceBank_limits *newLims);

/** @brief get the raw limits for this bank
 */
void bank_getLimits(Bank *bank, coyotos_SpaceBank_limits *newLims);

/** @brief get the usage of this bank
 */
void bank_getUsage(Bank *bank, coyotos_SpaceBank_limits *newLims);

/** @brief get the effective limits for this bank
 */
void bank_getEffLimits(Bank *bank, coyotos_SpaceBank_limits *newLims);

/** @brief Free an object bank to its bank. */
void object_unalloc(Object *obj);

/** @brief Free an object bank to its bank. */
void object_rescindAndFree(Object *obj);

/** @brief Identify an object from a strong capability to it. */
Object *object_identify(cap_t cap);

/** @brief Get a strong capability to a particular object */
void object_getCap(Object *obj, cap_t out);

extern int __assert3_fail(const char *file, int lineno, const char *desc, uint64_t val1, uint64_t val2);
extern int __assert_fail(const char *file, int lineno, const char *desc);

/** @brief crappy macro until we get a LOG key */
#undef assert
#define assert(x) ((void)((x) || __assert_fail(__FILE__, __LINE__, #x)))

#define assert3(a, x, b) ((void)({ \
		const typeof(a) __a = (a);\
		const typeof(b) __b = (b);\
		(__a x __b) || \
	__assert3_fail(__FILE__, __LINE__, #a " " #x " " #b, __a, __b);}))

#define assert3p(a, x, b) ((void)({ \
		const typeof(a) __a = (a);\
		const typeof(b) __b = (b);\
		(__a x __b) || \
		  __assert3_fail(__FILE__, __LINE__, #a " " #x " " #b, \
				 (uintptr_t)__a, (uintptr_t)__b);}))

#endif /* SPACEBANK_H__ */
