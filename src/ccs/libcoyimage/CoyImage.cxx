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
 * @brief Definition of the in-memory structure corresponding to a
 * Coyotos image file.
 */

#include <string.h>
#include <iostream>

#include "CoyImage.hxx"
#include <libsherpa/UExcept.hxx>
#include <libsherpa/ofBinaryStream.hxx>
#include <libsherpa/ifBinaryStream.hxx>

using namespace std;
using namespace sherpa;

static const uint64_t PhysPageStartOID = 0xff00000000000000ull;
const uint32_t CoyImage::FullStrengthBank = 0u;

/// standard integer division:  round up @p a to a multiple of @p b
static inline size_t
round_up(size_t a, size_t b)
{
  return ((a + b - 1)/b) * b;
}

/// standard integer division: how many @p b's to be @>= @p a?
static inline size_t
howmany(size_t a, size_t b)
{
  return ((a + b - 1)/b);
}

static bool
alloc_ascending(const CiAlloc& a1, const CiAlloc& a2)
{
  if (a1.fType < a2.fType)
    return true;
  if (a1.fType > a2.fType)
    return false;

  return (a1.oid < a2.oid);
}


CoyImage::CoyImage(const std::string& archName)
  : target(findArch(archName))
{
  //  nExternal = 0;
  iplOid = 0;

  // Allocate/initialize the header page. Note that this is the first
  // page allocation, so this will be page OID 0.
  (void) InitObjectInVector(0, vec.page, ct_Page, new CiPage(target.pageSize));

  // Allocate/initialize bank 0. Note that this is the first endpoint
  // allocation, so this will be endpoint OID 0.
  (void) AllocBankEndpoint(0);
}

CoyImage::CoyImage(uint32_t archNo)
  : target(findArch(archNo))
{
  //  nExternal = 0;
  iplOid = 0;

  // Allocate/initialize the header page. Note that this is the first
  // page allocation, so this will be page OID 0.
  (void) InitObjectInVector(0, vec.page, ct_Page, new CiPage(target.pageSize));

  // Allocate/initialize bank 0. Note that this is the first endpoint
  // allocation, so this will be endpoint OID 0.
  (void) AllocBankEndpoint(0);
}

capability
CoyImage::CiCap(CapType ct, oid_t oid)
{
  capability cap;

  memset(&cap, 0, sizeof(cap));
  cap.type = ct;

  if (cap_isMemory(cap))
    cap.u1.mem.l2g = target.pagel2v;

  if (cap_isObjectCap(cap))
      cap.u2.oid = oid;

  return cap;
}

void 
CoyImage::ToFile(sherpa::Path p)
{
  // Before doing anything, validate that the image looks correct.
  Validate();

  ofBinaryStream obs(p.c_str(), 
		     ios_base::out|ios_base::binary|ios_base::trunc);

  if (target.endian == LITTLE_ENDIAN)
    obs << bs_base::LittleEndian;
  else if (target.endian == BIG_ENDIAN)
    obs << bs_base::BigEndian;
  else
    THROW(excpt::BadValue, "PDP endian targets not supported.");
    
  if (sizeof(ExProcess) != OBSTORE_EXPROCESS_COMMON_SIZE)
    THROW(excpt::BadValue, 
	  format ("Process structure size mismatch."
		  "Expected %d, got %d",
		  sizeof(ExProcess), OBSTORE_EXPROCESS_COMMON_SIZE));


  // Remember the number of allocated pages before we expanded things:
  size_t originalPages = vec.page.size();

  // Pre-allocate the pages for bank records. Serialized bank records
  // are 16 bytes (8 oid, 8 parent). Fortunately these do
  // not grow during serialization:
  size_t nBankPages = howmany(vec.bank.size() * 16, target.pageSize);
  for (size_t i = 0; i < nBankPages; i++)
    InitObjectInVector(0, vec.page, ct_Page, new CiPage(target.pageSize));

  // For the moment we do not deal with external symbols or their
  // strings. When we do, we will pre-allocate them here.

  // Pre-allocate the pages for the allocation records. Allocation
  // records are 24 bytes (8 OID, 8 bankID, 4 frameType, 4 PAD). Note that
  // there is a convergence issue here, since allocating a new page
  // introduces an allocation record, and this may in turn cause the
  // allocation vector to grow.
  size_t nAllocPages = 
    howmany(vec.alloc.size() * CiAlloc::AllocEntrySize, target.pageSize);

  for (size_t i = 0; i < nAllocPages; i++) {
    InitObjectInVector(0, vec.page, ct_Page, new CiPage(target.pageSize));
    // Deal with convergence issue:
    nAllocPages = 
      howmany(vec.alloc.size() * CiAlloc::AllocEntrySize, target.pageSize);
  }

  uint32_t totalBytes = 
    vec.page.size() * target.pageSize +
    vec.capPage.size() * target.pageSize +
    vec.gpt.size() * sizeof(ExGPT) +
    vec.endpt.size() * sizeof(ExEndpoint) +
    vec.proc.size() * sizeof(ExProcess);

  // We now have the necessary information to write the image. Write
  // the header BY HAND:

  obs << "coyimage"		// magic string
      << target.endian          // target-specific endian value
      << (uint32_t) 1		// image version number
      << target.no		// target architecture
      << target.pageSize	// target page size
      << (uint32_t) vec.alloc.size()
      << (uint32_t) vec.bank.size()
      << (uint32_t) 0 		// extern sym table (not implemented)
      << (uint32_t) 0		// string table size (not implemented)
      << (uint32_t) vec.page.size()
      << (uint32_t) vec.capPage.size()
      << (uint32_t) vec.gpt.size()
      << (uint32_t) vec.endpt.size()
      << (uint32_t) vec.proc.size()
      << (uint32_t) totalBytes

      << (uint64_t) originalPages // bank table start
      << (uint64_t) originalPages + nBankPages // symbol table start
      << (uint64_t) originalPages + nBankPages + 0 // string table start
      << (uint64_t) originalPages + nBankPages + 0 + 0 // alloc table start
      << (uint64_t) originalPages + nBankPages + 0 + 0 + nAllocPages; // end

  // End of page 0 content. Align up to end of page:
  obs.align(target.pageSize);

  // Write the objects in order of object frame type number 
  {
    // We need to handle the page vector specially. Page 0 has already
    // been written (the header):
    for (size_t i = 1; i < originalPages; i++)
      obs << *vec.page[i];

    obs.align(target.pageSize);

    // Write the bank vector:
    for (size_t i = 0; i < vec.bank.size(); i++)
      obs << vec.bank[i];

    obs.align(target.pageSize);

    // Skip the symbol and string tables for now. Sort the alloc table
    // and write it:
    std::sort(vec.alloc.begin(), vec.alloc.end(), alloc_ascending);
    for (size_t i = 0; i < vec.alloc.size(); i++)
      obs << vec.alloc[i];

    // Round up to page boundary:
    obs.align(target.pageSize);
  }

  for (size_t i = 0; i < vec.capPage.size(); i++)
    obs << *vec.capPage[i];

  // Round up to page boundary (should already be there):
  obs.align(target.pageSize);

  for (size_t i = 0; i < vec.gpt.size(); i++)
    obs << *vec.gpt[i];

  for (size_t i = 0; i < vec.endpt.size(); i++)
    obs << *vec.endpt[i];

  for (size_t i = 0; i < vec.proc.size(); i++)
    obs << *vec.proc[i];

  if (obs.tellp() != (std::streampos) totalBytes) {
    p.remove();
    THROW(excpt::IoError, 
	  format("Image size %d does not match predicted length %d. Image removed.",
		 (unsigned long) obs.tellp(), totalBytes));
  }
    
  obs.close();

  // Truncate the allocation vector back to the correct size:
  {
    size_t delta = vec.page.size() - originalPages;
    for (size_t i = originalPages; i < (vec.alloc.size() - delta); i++)
      vec.alloc[i] = vec.alloc[i + delta];

    vec.alloc.resize(vec.alloc.size() - delta);

    // Truncate the page vector back to the correct size:
    vec.page.resize(vec.page.size() - delta);
  }
}

#if 0
sherpa::GCPtr<CoyImage>
CoyImage::FromFile(sherpa::Path p)
{
  ifBinaryStream ibs(p.c_str(), ios_base::binary);

  ibs >> bs_base::LittleEndian;	// set stream behavior

  uint32_t endian, version;
  std::string magic;

  ibs.readString(8, magic);
  if (magic != "coyimage")
    THROW(excpt::BadValue, "Image has bad magic string");

  ibs >> endian;

  if (endian != bs_base::LittleEndian)
    THROW(excpt::Malformed, "Image has bad endian value");

  ibs >> version;
  if (version != 1)
    THROW(excpt::BadValue, 
	  format("Image has unsupported version number %d", version));

  uint32_t target;

  ibs >> target;

  GCPtr<CoyImage> ci = new CoyImage(target);
  
  ibs >> ci->iplOid;

  // Write sizes of various vectors:
  uint32_t symbol_size;
  uint32_t bank_size;
  uint32_t gpt_size;
  uint32_t endp_size;
  uint32_t proc_size;
  uint32_t page_size;
  uint32_t capPage_size;

  ibs >> symbol_size
      >> bank_size
      >> gpt_size
      >> endp_size
      >> proc_size
      >> page_size
      >> capPage_size
    ;

  for (size_t i = 0; i < symbol_size; i++) {
    Symbol s;
    ibs >> s;
    ci->vec.symbol.push_back(s);
  }

  for (size_t i = 0; i < bank_size; i++) {
    Bank b;
    ibs >> b;
    ci->vec.bank.push_back(b);
  }

  for (size_t i = 0; i < gpt_size; i++) {
    ci->vec.gpt.push_back(new CiGPT(ci->target.pagel2v));
    ibs >> *ci->vec.gpt[i];
  }

  for (size_t i = 0; i < endp_size; i++) {
    ci->vec.endpt.push_back(new CiEndpoint);
    ibs >> *ci->vec.endpt[i];
  }

  for (size_t i = 0; i < proc_size; i++) {
    ci->vec.proc.push_back(new CiProcess);
    ibs >> *ci->vec.proc[i];
  }

  for (size_t i = 0; i < page_size; i++) {
    ci->vec.page.push_back(new CiPage(ci->target.pageSize));
    ibs >> *ci->vec.page[i];
  }

  for (size_t i = 0; i < capPage_size; i++) {
    ci->vec.capPage.push_back(new CiCapPage(ci->target.pageSize));
    ibs >> *ci->vec.capPage[i];
  }

  ibs.close();

  return ci;
}
#endif

/** @Brief Allocate a new space bank having the specified parent bank.
 *
 * We can't use AllocEndpoint here, because that requires a valid bank
 * capability, and the endpoint we are allocating here needs to
 * self-reference the bank that we are implicitly creating.
 */
capability
CoyImage::AllocBankEndpoint(bankid_t parent)
{
  bankid_t theBankID = vec.bank.size();

  capability bankCap =
    InitObjectInVector(theBankID, vec.endpt, ct_Endpoint, new CiEndpoint);

  GCPtr<CiEndpoint> ep = GetEndpoint(bankCap);

  // epID 0 is reserved for the Reply endpoint, so offset by 1
  ep->v.endpointID = theBankID + 1;

  vec.bank.push_back(CiBank(ep->oid, parent));

  return bankCap;		// this is the ENDPOINT cap
}

capability
CoyImage::AllocBank(capability bank)
{
  capability cap = AllocBankEndpoint(BankCapToBankID(bank));
  cap.u1.protPayload = 0;
  cap.type = ct_Entry;
  return cap;
}

bankid_t 
CoyImage::BankCapToBankID(const capability& bankCap)
{
  GCPtr<CiEndpoint> ep = GetEndpoint(bankCap);
  return ep->v.endpointID - 1;
}

/** @brief Return the bank from which an object was allocated.
 *
 * @bug This is a placeholder implementation.
 */
capability
CoyImage::GetObjectBank(const capability& cap)
{
  bankid_t theBankID;

  switch(cap.type) {
  case ct_Endpoint:
    {
      GCPtr<CiEndpoint> ob = GetEndpoint(cap);
      theBankID = ob->bank;
      break;
    }
  case ct_Page:
    {
      GCPtr<CiPage> ob = GetPage(cap);
      theBankID = ob->bank;
      break;
    }
  case ct_CapPage:
    {
      GCPtr<CiCapPage> ob = GetCapPage(cap);
      theBankID = ob->bank;
      break;
    }
  case ct_GPT:
    {
      GCPtr<CiGPT> ob = GetGPT(cap);
      theBankID = ob->bank;
      break;
    }
  case ct_Process:
    {
      GCPtr<CiProcess> ob = GetProcess(cap);
      theBankID = ob->bank;
      break;
    }

  default:
    THROW(excpt::Assert, 
	  "GetObjectBank() does not operate on non-object capabilities.");
  }

  // We now have the bank id.
  if (theBankID >= vec.bank.size())
    THROW(excpt::Assert,
	  "Bank number out of range");

  CiBank b = vec.bank[theBankID];

  capability tmpCap = CiCap(ct_Endpoint, b.oid);
  GCPtr<CiEndpoint> ep = GetEndpoint(tmpCap);
  ep->v.endpointID = theBankID;

  capability sndCap(tmpCap);
  sndCap.type = ct_Entry;
  sndCap.u1.protPayload = CoyImage::FullStrengthBank;

  return sndCap;
}

/**
 * @brief Allocate and install a new object @p obj for vector @p v,
 * allocated from @p bank, and return a capability to the new object,
 * which is of type @p t.
 *
 * One of a pair of templated routines which regularize primitive object
 * allocation.
 */
template <class T> inline capability 
CoyImage::InitObjectInVector(bankid_t theBankID,
			     std::vector<sherpa::GCPtr<T> > &v, 
			     CapType t, T *obj)
{
  uint64_t oid = v.size();
  v.push_back(obj);

  obj->oid = oid;

  vec.alloc.push_back(CiAlloc(cap_frameType(t), oid, theBankID));

  return CiCap(t, oid);
}

/** @brief Typecheck a capability and return the object it points to
 *
 * For object that only have a single valid capability type, @p t2 should
 * be the same as @p t1.
 */
template <class T> inline T
CoyImage::GetObjectInVector(std::vector<T> &v, const capability &cap,
			    CapType t1, CapType t2)
{
  if ((CapType)cap.type != t1 && (CapType)cap.type != t2)
    THROW(excpt::BadValue, "Wrong capability type");

  // relies on the type pun for the OID field
  uint64_t oid = cap.u2.oid;

  if (oid >= v.size())
    THROW(excpt::BoundsError, "Array bound exceeded");

  return v[oid];
}

/** @brief Allocate an initially empty GPT. */
capability
CoyImage::AllocGPT(capability bank)
{
  return InitObjectInVector(bank, vec.gpt, ct_GPT, new CiGPT(target.pagel2v));
}

GCPtr<CiGPT>
CoyImage::GetGPT(const capability& cap)
{
  return GetObjectInVector(vec.gpt, cap, ct_GPT, ct_GPT);
}

/** @brief Allocate an initially empty endpoint.
 *
 * @note If this is updated, also may need to update AllocBank()
 */
capability
CoyImage::AllocEndpoint(capability bank)
{
  return InitObjectInVector(bank, vec.endpt, ct_Endpoint, new CiEndpoint);
}

GCPtr<CiEndpoint>
CoyImage::GetEndpoint(const capability& cap)
{
  return GetObjectInVector(vec.endpt, cap, ct_Endpoint, ct_Entry);
}

/** @brief Allocate an initially empty Process. */
capability
CoyImage::AllocProcess(capability bank)
{
  return InitObjectInVector(bank, vec.proc, ct_Process, new CiProcess);
}

GCPtr<CiProcess>
CoyImage::GetProcess(const capability& cap)
{
  return GetObjectInVector(vec.proc, cap, ct_Process, ct_Process);
}

/** @brief Allocate an initially empty Page. */
capability
CoyImage::AllocPage(capability bank)
{
  return InitObjectInVector(bank, vec.page, ct_Page, new CiPage(target.pageSize));
}

GCPtr<CiPage>
CoyImage::GetPage(const capability& cap)
{
  return GetObjectInVector(vec.page, cap, ct_Page, ct_Page);
}

/** @brief Allocate an initially empty Capability Page. */
capability
CoyImage::AllocCapPage(capability bank)
{
  return InitObjectInVector(bank, vec.capPage, 
			    ct_CapPage, new CiCapPage(target.pageSize));
}

GCPtr<CiCapPage>
CoyImage::GetCapPage(const capability& cap)
{
  return GetObjectInVector(vec.capPage, cap, ct_CapPage, ct_CapPage);
}

  /** @brief Do validity checks on a capability */
void
CoyImage::ValidateCap(const capability &cap, 
		      const char *container, 
		      size_t container_id, 
		      size_t slot_id) const
{
  // No on-disk caps can be prepared.
  if (cap.swizzled) {
      THROW(excpt::IntegrityFail,
	    format("%s #%d: cap %d is marked as swizzled.",
		   container, container_id, slot_id));
  }

  // The capability type must be valid
  if (!cap_isValidType(cap.type)) {
      THROW(excpt::IntegrityFail,
	    format("%s #%d: cap %d has unrecognized type %d",
		   container, container_id, slot_id, cap.type));
  }

  // For non-object, non-ct_Window caps, allocCount must be zero
  if (cap.allocCount != 0 && cap.type != ct_Window && !cap_isObjectCap(cap)) {
      THROW(excpt::IntegrityFail,
	    format("%s #%d: cap %d has non-zero allocCount %d",
		   container, container_id, slot_id, cap.allocCount));
  }

  if (! (cap_isObjectCap(cap) || cap_isWindow(cap) || 
	 cap_isEntryCap(cap)) ) {
    if (cap.u2.oid != 0)
      THROW(excpt::IntegrityFail,
	    format("%s #%d: cap %d has non-zero oid 0x%llx",
		   container, container_id, slot_id, cap.u2.oid));
  }

  // verify that the OID is in the correct range.
  if (cap_isObjectCap(cap)) {
    oid_t maxoid = 0;
    switch (cap.type) {
    case ct_Endpoint:
      maxoid = vec.endpt.size();
      break;
    case ct_Page:
      maxoid = vec.page.size();
      if (cap.u2.oid >= PhysPageStartOID)
	maxoid = ~0ull;
      break;
    case ct_CapPage:
      maxoid = vec.capPage.size();
      break;
    case ct_GPT:
      maxoid = vec.gpt.size();
      break;
    case ct_Process:
      maxoid = vec.proc.size();
      break;
    default:
      THROW(excpt::IntegrityFail,
	    format("%s #%d: cap %d has unexpected type %d",
		   container, container_id, slot_id));
      return;
    }
    if (cap.u2.oid >= maxoid) {
      THROW(excpt::IntegrityFail,
	    format(
	       "%s #%d: cap %d has out-of-bounds oid %lld (should be < %lld)",
	       container, container_id, slot_id, cap.u2.oid, maxoid));
    }
  }

  if (!cap_isMemory(cap)) {
    if (cap.restr != 0) {
      THROW(excpt::IntegrityFail,
	    format(
	       "%s #%d: cap %d has non-zero reserved bits in restr (0x%x)",
	       container, container_id, slot_id, cap.restr));
    }
    if (cap.type != ct_Endpoint && 
	cap.type != ct_Entry && 
	cap.type != ct_AppNotice && 
	cap.u1.protPayload != 0) {
      THROW(excpt::IntegrityFail,
	    format(
	       "%s #%d: cap %d has non-zero reserved bits in "
	       "protPayload (0x%x)",
	       container, container_id, slot_id, cap.u1.protPayload));
    }
  } else {
    /* is a memory cap */
    if ((cap.restr & (CAP_RESTR_OP|CAP_RESTR_NC)) && 
	!((cap.type == ct_GPT) ||
	  (cap.type == ct_Page) && (cap.u2.oid >= PhysPageStartOID))) {
      THROW(excpt::IntegrityFail,
	    format(
		   "%s #%d: cap %d has OP or NC set in restr (0x%x), but isn't a GPT cap",
		   container, container_id, slot_id, cap.restr));
    }
    if ((cap.restr & CAP_RESTR_WK) && !(cap.restr & CAP_RESTR_RO)) {
      THROW(excpt::IntegrityFail,
	    format(
	      "%s #%d: cap %d has WK but not RO set in restr (0x%x)",
	       container, container_id, slot_id, cap.restr));
    }

    if (cap.u1.mem.l2g > (8 * sizeof(coyaddr_t)) || 
	cap.u1.mem.l2g < target.pagel2v) {
      THROW(excpt::IntegrityFail,
	    format(
	       "%s #%d: cap %d has invalid l2g %d (should be %d <= l2g <= %d)",
	       container, container_id, slot_id, cap.u1.mem.l2g,
	       target.pagel2v, (8 * sizeof(coyaddr_t))));
    }
    coyaddr_t guard = 
      ((coyaddr_t)cap.u1.mem.match << 1) << (cap.u1.mem.l2g - 1);
    if (cap.u1.mem.match != ((guard >> 1) >> (cap.u1.mem.l2g - 1))) {
      THROW(excpt::IntegrityFail,
	    format(
	       "%s #%d: cap %d has guard with bits above %d: "
	       "l2g %d, match %x != truncated match %x",
	       container, container_id, slot_id, (8 * sizeof(coyaddr_t)),
	       cap.u1.mem.l2g, cap.u1.mem.match,
	       ((guard >> 1) >> (cap.u1.mem.l2g - 1))));
    }
  }

  if (cap_isWindow(cap)) {
    coyaddr_t mask = ((coyaddr_t)2 << (cap.u1.mem.l2g - 1)) - 1;
    if (cap.u2.offset & mask) {
      THROW(excpt::IntegrityFail,
	    format(
	       "%s #%d: cap %d has an offset %llx with bits below its l2g %d",
	       container, container_id, slot_id, 
	       cap.u2.offset, cap.u1.mem.l2g));
    }
    
    size_t maxCap = (cap.type == ct_Window)? (NUM_GPT_SLOTS - 1) : 0;
    if (cap.allocCount > maxCap) {
      THROW(excpt::IntegrityFail,
	    format(
	       "%s #%d: window cap %d has an invalid allocCount %d "
	       "(should be <= %d)",
	       container, container_id, slot_id,
	       cap.allocCount, maxCap));
    }
  }
}

/** @brief Run validity checks on the entire image */
void
CoyImage::Validate() const
{
  /*
   * This code (plus the ValidateCap code above) needs to, at a
   * minumum, capture all Capability and object correctness conditions
   * that the kernel assumes are always true.
   */
  // should this be validating that "bank" fields match?

  for (size_t i = 0; i < vec.page.size(); i++) {
    GCPtr<CiPage> page = vec.page[i];
    // any validity checks?
  }

  for (size_t i = 0; i < vec.capPage.size(); i++) {
    GCPtr<CiCapPage> capPage = vec.capPage[i];
    for (size_t j = 0; j < (capPage->pgSize / sizeof(capability)); j++) {
      ValidateCap(capPage->data[j], "CapPage", i, j);
    }
  }

  for (size_t i = 0; i < vec.gpt.size(); i++) {
    GCPtr<CiGPT> gpt = vec.gpt[i];
    for (size_t j = 0; j < NUM_GPT_SLOTS; j++) {
      ValidateCap(gpt->v.cap[j], "GPT", i, j);
    }
    if (gpt->v.l2v >= (8 * sizeof(coyaddr_t)) || gpt->v.l2v < target.pagel2v) {
      THROW(excpt::IntegrityFail,
	    format("GPT #%d has invalid l2v %d (should be %d <= l2v < %d)",
		   i, gpt->v.l2v, target.pagel2v, (8 * sizeof(coyaddr_t))));
    }
  }

  for (size_t i = 0; i < vec.endpt.size(); i++) {
    GCPtr<CiEndpoint> endpt = vec.endpt[i];

    if (endpt->v.recipient.type != ct_Null && 
	endpt->v.recipient.type != ct_Process) {
      THROW(excpt::IntegrityFail,
	    format("Endpoint #%d has non-Null, non-Process recipient", i));
    }

    ValidateCap(endpt->v.recipient, "Endpoint", i, 0);
  }

  for (size_t i = 0; i < vec.proc.size(); i++) {
    GCPtr<CiProcess> proc = vec.proc[i];

    if (proc->v.ioSpace.type != ct_Null && 
	proc->v.ioSpace.type != ct_IOPriv) {
      THROW(excpt::IntegrityFail,
	    format("Process #%d has non-Null, non-IOPriv ioSpace", i));
    }
    
    for (size_t j = 0; j < NUM_CAP_REGS; j++)
      ValidateCap(proc->v.capReg[j], "Process[CR]", i, j);
    ValidateCap(proc->v.schedule, "Process[sched]", i, 0);
    ValidateCap(proc->v.addrSpace, "Process[addrSpace]", i, 0);
    ValidateCap(proc->v.brand, "Process[brand]", i, 0);
    ValidateCap(proc->v.cohort, "Process[cohort]", i, 0);
    ValidateCap(proc->v.ioSpace, "Process[ioSpace]", i, 0);
    ValidateCap(proc->v.handler, "Process[handler]", i, 0);

    // validate FaultCode/FaultInfo/flags?
  }

}


