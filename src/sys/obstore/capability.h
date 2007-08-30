#ifndef __OBSTORE_CAPABILITY_H__
#define __OBSTORE_CAPABILITY_H__
/*
 * Copyright (C) 2006, The EROS Group, LLC.
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

#include <stdbool.h>
#include <inttypes.h>
#include <coyotos/coytypes.h>

/** @file
 * @brief Layout and essential definitions for capabilities.
 */

// The ANSI C standard does not define whether the underlying type of
// an enum is signed or unsigned, but it is explicit that enum
// constant values are signed. It is necessary to declare the
// constants themselves in an enum so that we can switch on them (or
// make them #defines, but we can't do that in the preprocessor). It
// is necessary NOT to use the enum tag as the type name, because many
// compilers will assign that a signed type. This is all rather
// unfortunate, because it utterly defeats the compiler's attempts to
// ensure that switch statements dispatching over capability types do
// not have missing cases.

#define DEFCAP(ty, ft, val) ct_##ty = val,
enum {
#include "captype.def"
};
typedef uint32_t CapType;

#define CAP_MATCH_WIDTH 24

#define CAP_MATCH_LIMIT ((1u << CAP_MATCH_WIDTH) - 1)

/** @brief capability restr field bit for Weak restriction */
#define CAP_RESTR_WK       0x01
/** @brief capability restr field bit for Read-Only restriction */
#define CAP_RESTR_RO       0x02
/** @brief capability restr field bit for No-Execute restriction */
#define CAP_RESTR_NX       0x04
/** @brief capability restr field bit for Opaque restriction */
#define CAP_RESTR_OP       0x08
/** @brief capability restr field bit for no-call restriction */
#define CAP_RESTR_NC       0x10
/** @brief capability restr field bit for resume-only process cap */
#define CAP_RESTR_RESTART  0x10

#define CAP_RESTR_MASK 0x1f

typedef struct capability {
  /** @brief Capability type code.
   *
   * This field determines the interpretation of the rest of the
   * capability structure.
   */
  uint32_t type : 6;

  /** @brief Capability is in in-memory form. */
  uint32_t swizzled  : 1;

  /** @brief Capability restrictions */
  uint32_t restr :5;

  uint32_t allocCount : 20;	/**< @brief Holds "slot" in local window cap.
				 * Not for window or misc cap */

  /** @brief Union 1:  protected payload and memory information. */
  union {
    uint32_t   protPayload;     /**< @brief Prot. Payload, for Entry caps */
    /** @brief Memory object information.
     *
     * Used for GPT, Page, CapPage, Window, and Background capabilities.
     */
    struct {
      uint32_t l2g : 7;
      uint32_t : 1;
      uint32_t match : CAP_MATCH_WIDTH;
    } mem;
  } u1;

  /** @brief Union 2:  OID, prepared object, or offset */
  union {
    /** @brief OID of object referenced.  
     *
     * Used by unprepared Object capabilities 
     */
    oid_t      oid;
#ifdef __KERNEL__
    /** @brief Prepared object structure.  
     *
     * Always used when "swizzled" is set.
     */
    struct {
      struct OTEntry *otIndex;
      struct ObjectHeader *target;
    } prepObj;
#endif /* __KERNEL__ */
    coyaddr_t offset;		/**< @brief Offset for Window, Background. */
  } u2;
} capability;

#endif /* __OBSTORE_CAPABILITY_H__ */
