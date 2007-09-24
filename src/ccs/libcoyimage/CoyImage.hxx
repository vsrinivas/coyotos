#ifndef LIBCOYIMAGE_COYIMAGE_HXX
#define LIBCOYIMAGE_COYIMAGE_HXX
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
 *
 * A Coyotos image file is serialized as a vector of object payloads,
 * pages first. By "payload", I mean that the serialized objects do
 * @b not include their OID and allocation counts (the ExHdr data).
 *
 * The object vectors are serialized in order sorted by type. Within a
 * type, they appear at increasing OIDs starting from zero. This
 * causes pages to be serialized first.  The allocation records will
 * be sorted in the same way, so they correspond positionally to the
 * objects. OIDs increase sequentially. Note this means that if
 * <tt>mkimage</tt> ever adds the ability to destroy objects, the
 * object will continue to exist in the image but will have an
 * incremented allocation count.
 *
 * By construction, the first page to be serialized is the image
 * header page (OID=0), which also serves as the information page that
 * is provided to the prime bank. In consequence, the nXXX fields
 * (nProc, nGPT, etc.) in the header have the effect of telling us the
 * object vector lengths.
 *
 * Just before serializing the CoyImage structure to a file, we
 * allocate additional pages to hold the bank records, the symbol
 * records, and the allocation records (in that order). Each of these
 * vectors will be serialized to sequential pages, beginning on a new
 * page. We do allocation records last because the bank and external
 * symbol records will need to allocate pages. So will the allocation
 * records, but at least by doing them last we avoid convergence
 * issues.
 *
 * The header page, Alloc structures, and Bank structure are
 * serialized in TARGET ENDIAN order.
 */

#include <stddef.h>
#include <vector>
#include <string>

#include <obstore/GPT.h>
#include <obstore/Endpoint.h>
#include <obstore/Process.h>
#include <obstore/CoyImgHdr.h>

#include <libsherpa/GCPtr.hxx>
#include <libsherpa/Path.hxx>
#include <libsherpa/oBinaryStream.hxx>
#include <libsherpa/iBinaryStream.hxx>

#include "CiCap.hxx"
#include "CiBank.hxx"
#include "CiAlloc.hxx"
#include "CiSymbol.hxx"
#include "CiGPT.hxx"
#include "CiEndpoint.hxx"
#include "CiProcess.hxx"
#include "CiPage.hxx"
#include "CiCapPage.hxx"

struct ArchInfo {
  const std::string name;
  uint32_t          no;		// architecture number
  uint32_t          pageSize;	// in bytes
  uint32_t	    pagel2v;	// log2(pageSize)
  uint16_t          elfMachine;	// ELF machine id
  uint32_t          endian;
};

/// @brief CoyImage -- portions of the CoyImage functionality
/// that are not machine dependent.
class CoyImage : public sherpa::Countable {
protected:
  struct TraverseRet {
    capability& slot;
    coyaddr_t offset;

    TraverseRet(capability& s, coyaddr_t _offset)
      : slot(s), offset(_offset) {}
  };

  void
  split_GPT(capability bank, sherpa::GCPtr<CiGPT>, coyaddr_t, size_t);

  struct TraverseRet
  traverse_to_slot(capability bank, capability& space, coyaddr_t offset,
		   size_t l2arg, uint32_t restr);

  bankid_t BankCapToBankID(const capability& bank);
  capability AllocBankEndpoint(bankid_t parent);

  template <class T> inline capability
  InitObjectInVector(bankid_t bank,
		     std::vector<sherpa::GCPtr<T> > &v,
		     CapType t, T *obj);

  template <class T> inline capability
  InitObjectInVector(const capability& bank,
		     std::vector<sherpa::GCPtr<T> > &v,
		     CapType t, T *obj)
  {
    return InitObjectInVector(BankCapToBankID(bank), v, t, obj);
  }

  template <class T> inline T
  GetObjectInVector(std::vector<T> &v, const capability& cap,
		    CapType t1, CapType t2);

  /** @brief Do validity checks on a capability */
  void	     ValidateCap(const capability &cap, const char *container, 
			 size_t container_id, size_t slot_id)  const;

public:
  static const uint32_t FullStrengthBank;

  /** @brief Number of external capability references. */
  //  uint32_t                nExternal;

  /// @brief Target architecture.
  const ArchInfo&         target; // pointer to static!

  /** @brief OID of the IPL domain. 
   *
   * Should this be a process capability? */
  oid_t                   iplOid;


  // std::vector<char>       strTable;

  struct {
    std::vector<Symbol>      symbol; // externs
    std::vector<CiBank>      bank;
    std::vector<CiAlloc>     alloc;

    std::vector< sherpa::GCPtr<CiGPT> >      gpt;
    std::vector< sherpa::GCPtr<CiEndpoint> > endpt;
    std::vector< sherpa::GCPtr<CiProcess> >  proc;
    std::vector< sherpa::GCPtr<CiPage> >     page;
    std::vector< sherpa::GCPtr<CiCapPage> >  capPage;
  } vec;

  static const ArchInfo& findArch(const std::string&);
  static const ArchInfo& findArch(uint32_t archNo);

  CoyImage(const std::string& archName);
  CoyImage(uint32_t archNo);

  /// @brief Initializer for capabilities.
  inline capability CiCap()
  {
    capability cap;
    memset(&cap, 0, sizeof(cap));
    return cap;
  }

  /// @brief Initializer for other capabilities.
  ///
  /// This is machine dependent because the l2v value for page
  /// capabilities is architecture dependent.
  capability CiCap(CapType ct, oid_t oid = 0);

  bool isBankCap(capability cap);

  capability GetObjectBank(const capability& ob);
  
  void       BindSym(const std::string&, const capability&);
  capability Lookup(const std::string&);

#if 0
  static sherpa::GCPtr<CoyImage> FromFile(sherpa::Path);
#endif
  void ToFile(sherpa::Path);

  capability AllocBank(capability bank);

  capability AllocEndpoint(capability bank);
  capability AllocGPT(capability bank);
  capability AllocProcess(capability bank);
  capability AllocPage(capability bank);
  capability AllocCapPage(capability bank);

  sherpa::GCPtr<CiGPT>      GetGPT(const capability&);
  sherpa::GCPtr<CiEndpoint> GetEndpoint(const capability&);
  sherpa::GCPtr<CiProcess>  GetProcess(const capability&);
  sherpa::GCPtr<CiPage>     GetPage(const capability&);
  sherpa::GCPtr<CiCapPage>  GetCapPage(const capability&);

public:

  capability  MakeGuardedSubSpace(capability bank, capability spc, 
				  coyaddr_t offset, size_t l2arg);
#if 0
  ci_result_t AddProgramSpace(CoyImage *image,
			      const char *fileName,
			      const capability& *newSpace);
  ci_result_t AddRawSpace(CoyImage *image,
			  const char *fileName,
			  const capability& *newSpace);
  ci_result_t AddZeroSegment(CoyImage *image,
			     coyaddr_t nPages,
			     const capability& *newSpace);
  ci_result_t AddEmptySpace(CoyImage *image,
			    coyaddr_t nPages,
			    const capability& *newSpace);
#endif

  /** @brief Adds @p subSpace to @p space, returning the new space root.
    *
    * Adds @p subSpace to @p space at @p offset, as if it were @p l2arg in
    * size.  Returns the new root of the space.  @p space and its contents
    * will be modified as necessary to process this request.
    *  
    * @p space and its contents may be modified <b>even if</b> an
    * exception is thrown.  If that happens, only the <b>structure</b> will
    * change, the content and used addresses will remain the same.
    *
    * Note that any guard on @p subSpace is ignored by this algorithm, and
    * the actual inserted capability will have a guard determined by @p offset
    * and its position in the address tree.
    *
    * @p where must be aligned on a @p l2arg boundry.  If @p l2arg is zero,
    * it will be derived from the size of the address space @p subSpace covers
    * (ignoring its guard).
    */
  capability AddSubSpaceToSpace(capability bank, capability space,
				coyaddr_t where,
				capability subSpace, size_t l2arg = 0);
  
  struct LeafInfo {
    capability cap;
    coyaddr_t offset; // offset within cap

    uint32_t restr; // path+cap restrictions

    LeafInfo(capability _cap, coyaddr_t _offset, uint32_t _restr)
      : cap(_cap), offset(_offset), restr(_restr)
    {}
  };
  /*
   * @brief Retrieve the Page or CapPage capability at offset @p where
   * in @p space.  Returns the capability, any remaining offset, and
   * the restrictions placed on the cap along the path.
   */
  LeafInfo LeafAtAddr(const capability &space, coyaddr_t where);

#if 0

  // ??? Shouldn't these be generalized?
  void       SetPageWord(CoyImage *ei, const capability& pageKey, 
			 coyaddr_t offset, uint32_t value);

  void       SetPageCap(CoyImage *ei, const capability& capPageKey, 
			coyaddr_t offset, const capability& value);

#endif

  /** @brief Run validity checks on the entire image */
  void	     Validate() const;

  size_t     l2offset(coyaddr_t);
  size_t     l2cap(const capability&);
};

#endif /* LIBCOYIMAGE_COYIMAGE_HXX */
