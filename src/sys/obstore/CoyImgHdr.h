#ifndef __OBSTORE_COYIMGHDR_H__
#define __OBSTORE_COYIMGHDR_H__
/*
 * Copyright (C) 2006, The EROS Group, LLC.
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
 * @brief Coyotos image header. */

#include "ObFrameType.h"
typedef uint64_t bankid_t;

/// @brief Data structure to track object allocations in a Coyotos Image.
struct CoyImgAlloc {
  oid_t           oid;		// OID of allocated object
  bankid_t        bank;		// Owning bank
  uint32_t        fType;	// Object frame type (repr type)
  uint32_t        /* PAD */: 32;
};
typedef struct CoyImgAlloc CoyImgAlloc;

/// @brief Data structure to track the bank hierarchy.
struct CoyImgBank {
  oid_t      oid;		/* OID of bank endpoint */
  bankid_t   parent;		/* index of parent bank */
};
typedef struct CoyImgBank CoyImgBank;


/// @brief Information that we will pass to the space bank to support
/// its initialization.
///
/// This is an overlay structure that is written on to page zero of
/// the image file. It is always written in target-endian form.
/// This structure is also consulted by the prime bank during startup.
struct CoyImgHdr {
  char magic[8]; /* 'c' 'o' 'y' 'i' 'm' 'a' 'g' 'e' */

  uint32_t endian;
  uint32_t version;		// image version number
  uint32_t target;		// architecture number
  uint32_t pgSize;		// sanity check

  /// @brief Number of allocated object structures
  uint32_t nAlloc;
  /// @brief Number of allocated bank structures
  uint32_t nBank;
  /// @brief Number of external symbols
  uint32_t nSymbol;

  /// @brief Size of string table bytes, including trailing null.
  uint32_t nStringBytes;

  /// @brief Number of allocated Pages, including those for
  /// book-keeping structures.
  uint32_t nPage;
  /// @brief Number of CapPages
  uint32_t nCapPage;
  /// @brief Number of GPTs
  uint32_t nGPT;
  /// @brief Number of Endpoints
  uint32_t nEndpoint;
  /// @brief Number of Processes
  uint32_t nProc;

  uint32_t imgBytes;		/* Total image bytes */

  /// @brief Starting OID for bank structures.
  oid_t    bankVecOID __attribute__ ((aligned (8)));
  /// @brief Starting OID for external symbol structures
  oid_t    symVecOID __attribute__ ((aligned (8)));
  /// @brief Starting OID for string table.
  oid_t    stringTableOID __attribute__ ((aligned (8)));
  /// @brief Starting OID for allocation structures.
  oid_t    allocVecOID __attribute__ ((aligned (8)));
  /// @brief End OID for pages of metadata
  oid_t    endVecOID __attribute__ ((aligned (8)));
};
typedef struct CoyImgHdr CoyImgHdr;

#endif /* __OBSTORE_COYIMGHDR_H__ */
