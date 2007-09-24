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

#include <assert.h>
#include <iostream>
#include <iomanip>
#include <string>

#include <libsherpa/utf8.hxx>

#include "Environment.hxx"
#include "Value.hxx"
#include "AST.hxx"
#include "builtin.hxx"

using namespace sherpa;

template <class T> static void
pf_walkGPTTree(GCPtr<CoyImage> ci, const capability& cap,
	       void (*callback)(GCPtr<CoyImage>, int, 
				const capability&, const T &),
	       const T &cbdata)
{
  switch (cap.type) {
  case ct_GPT: {

    GCPtr<CiGPT> gpt = ci->GetGPT(cap);

    int i = 0;

    for (i = 0; i < NUM_GPT_SLOTS; i++)
      (*callback)(ci, i, gpt->v.cap[i], cbdata);

    break;
  }
    
  default:
    return;
  }
}

struct DumpTreeCBInfo {
  ostream &outStream;
  int depth;
  DumpTreeCBInfo(ostream &_outStream, int _depth) :
    outStream(_outStream), depth(_depth)
  {}
};

static void pf_doprint_tree_base(GCPtr<CoyImage> ci, std::ostream &outStream,
				 int depth, const capability& cap);

static void
pf_doprint_tree_cb(GCPtr<CoyImage> ci, int slot, const capability& cap,
		   const DumpTreeCBInfo &info)
{
  if (cap.type == ct_Null)
    return;

  info.outStream << setw(info.depth * 2) << ""
		 << setw(2) << setfill('0') << slot << setfill(' ')
		 << ": ";

  pf_doprint_tree_base(ci, info.outStream, info.depth, cap);
}

static void pf_doprint_tree_base(GCPtr<CoyImage> ci, std::ostream &outStream,
				 int depth, const capability& cap)
{
  outStream << cap << endl;
  if (cap.type == ct_GPT) {
    GCPtr<CiGPT> gpt = ci->GetGPT(cap);
    outStream << setw(depth * 2) << ""
	      << "{ GPT state: l2v=" << gpt->v.l2v
	      << " bg=" << gpt->v.bg
	      << " ha=" << gpt->v.ha
	      << " slots=\n";
    DumpTreeCBInfo info(outStream, depth + 1);
    pf_walkGPTTree(ci, cap, pf_doprint_tree_cb, info);
    outStream << setw(depth * 2) << ""
	      << "}\n";
  }
}

GCPtr<Value>
pf_doprint_tree(PrimFnValue& pfv,
		InterpState& is,
		sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> v0 = args[0];

  if (v0->kind != Value::vk_cap) {
    // Be forgiving:
    pf_print_one(is, v0);
    std::cout << '\n';
    return &TheUnitValue;
  }

  GCPtr<CapValue> cv = v0.upcast<CapValue>();
  GCPtr<CoyImage> ci = cv->ci;

  pf_doprint_tree_base(ci, std::cout, 0, cv->cap);

  return &TheUnitValue;
}

struct DumpSpaceCBInfo
{
  std::ostream &outStream;
  coyaddr_t offset;
  coyaddr_t maxoff;
  std::string path;
  int depth;

  uint32_t restr; // path permissions

  bool bg;	// from GPT
  bool ha;	// from GPT
  size_t l2v;	// from GPT

  DumpSpaceCBInfo(std::ostream &_outStream, std::string _path)
    : outStream(_outStream), offset(0), maxoff(-(coyaddr_t)1),
      path(_path), depth(1), 
      restr(0), bg(false), ha(false), l2v(0)
  {}
};


#define bitsizeof(x) (8 * sizeof (x))

static inline coyaddr_t 
max(coyaddr_t x, coyaddr_t y)
{
  return (x < y) ? y : x;
}

static inline coyaddr_t 
min(coyaddr_t x, coyaddr_t y)
{
  return (x > y) ? y : x;
}

static inline coyaddr_t
lowbits(coyaddr_t off, size_t bit)
{
  if (bit == bitsizeof(coyaddr_t))
    return off;
  return off & ((1ull << bit) - 1ull);
}

static inline coyaddr_t
hibits_shifted(coyaddr_t off, size_t bit)
{
  if (bit == bitsizeof(coyaddr_t))
    return 0;
  return off >> bit;
}

static inline coyaddr_t
shift_to_hibits(coyaddr_t off, size_t bit)
{
  if (bit == bitsizeof(coyaddr_t))
    return 0;
  return off << bit;
}

static void pf_doprint_space_base(GCPtr<CoyImage> ci, const capability& cap,
				  const DumpSpaceCBInfo &info);

void
pf_doprint_space_cb(GCPtr<CoyImage> ci, int slot, const capability& cap, 
		  const DumpSpaceCBInfo& info)
{
  ostringstream oss;
  oss << info.path << '[' << slot << ']';

  // missing: keeper / bg space handling

  coyaddr_t slotaddr = shift_to_hibits(slot, info.l2v);
  if (slotaddr > info.maxoff ||
      hibits_shifted(slotaddr, info.l2v) != (coyaddr_t)slot) {
    if (cap.type != ct_Null) {
      info.outStream << oss.str() << ": slot is unreachable" 
		     << " (slotaddr=" 
		     << std::hex << std::showbase << slotaddr << std::dec
		     << ", maxoff="
		     << std::hex << std::showbase << info.maxoff << std::dec
		     << ")" << std::endl
		     << oss.str() << ": " << cap << std::endl;
    }
    return;
  }

  DumpSpaceCBInfo myInfo = info;
  myInfo.offset |= slotaddr;
  myInfo.maxoff = lowbits(info.maxoff, info.l2v);
  myInfo.l2v = 0;
  myInfo.path = oss.str();
  myInfo.bg = 0;
  myInfo.ha = 0;

  pf_doprint_space_base(ci, cap, myInfo);
}

void
pf_doprint_space_base(GCPtr<CoyImage> ci, const capability& cap,
		      const DumpSpaceCBInfo &info)
{
  coyaddr_t maxoff = info.maxoff;
  coyaddr_t offset = info.offset;

  if (cap.type == ct_Null)
    return;

  // XXX missing: windows
  // XXX missing: keeper / window space

  if (cap_isMemory(cap) && !cap_isWindow(cap)) {
    coyaddr_t guard = cap_guard(cap);

    if (guard > maxoff) {
      std::cerr << info.path << " guard makes object unreachable ("
		<< guard << " > " << maxoff << ")" << std::endl;
      return;
    }
    offset |= guard;
    maxoff = lowbits(maxoff, cap.u1.mem.l2g);

    coyaddr_t maxoffset = offset + maxoff;

    uint8_t cap_restr = cap.restr;
    bool restr_cd = (cap.type == ct_Page && (cap_restr & CAP_RESTR_CD));
    bool restr_wt = (cap.type == ct_Page && (cap_restr & CAP_RESTR_WT));
    if (cap.type == ct_Page)
      cap_restr &= ~(CAP_RESTR_CD|CAP_RESTR_WT);

    info.outStream << setw(info.depth) << ""
		   << setfill('0')
		   << "0x" << setw(16) << hex << offset << dec << "-"
		   << "0x" << setw(16) << hex << maxoffset << dec << " "
		   << setfill(' ')
		   << ((cap_restr & CAP_RESTR_RO)? "RO " :
		       (info.restr & CAP_RESTR_RO)? "ro " : "   ")
		   << ((cap_restr & CAP_RESTR_NX)? "NX " :
		       (info.restr & CAP_RESTR_NX)? "nx " : "   ")
		   << ((cap_restr & CAP_RESTR_WK)? "WK " :
		       (info.restr & CAP_RESTR_WK)? "wk " : "   ")
		   << ((cap_restr & CAP_RESTR_OP)? "OP " :
		       (info.restr & CAP_RESTR_OP)? "op " : "   ");
    
    if (restr_cd)
      info.outStream << "CD ";
    if (restr_wt)
      info.outStream << "WT ";

    info.outStream << ' '
		   << cap_typeName(cap) << " 0x" << hex << cap.u2.oid << dec;
    
    if (cap.type != ct_GPT) {
      info.outStream << endl;
      return;
    }
    
    GCPtr<CiGPT> capGPT = ci->GetGPT(cap);

    size_t l2g = cap.u1.mem.l2g;
    size_t l2v = capGPT->v.l2v;
    size_t slots = 1 << min(l2g - l2v, GPT_SLOT_INDEX_BITS);
    size_t reachable = min(slots, hibits_shifted(maxoff, l2v) + 1);

    info.outStream << ", " << dec
		   << "l2g=" << l2g << " l2v=" << l2v << " slots="
		   << slots;
    if (reachable < slots)
      info.outStream << ", " << reachable << " reachable";
    info.outStream << endl;
    
    info.outStream << setw(info.depth) << "" << "{" << endl;

    DumpSpaceCBInfo myInfo = info;
    myInfo.offset = offset;
    myInfo.maxoff = maxoff;
    myInfo.depth = info.depth + 1;
    myInfo.restr |= cap.restr;
    myInfo.bg = capGPT->v.bg;
    myInfo.ha = capGPT->v.ha;
    myInfo.l2v = l2v;

    pf_walkGPTTree(ci, cap, pf_doprint_space_cb, myInfo);
    cout << setw(info.depth) << "" << "}" << endl;
    return;
  }

  cout << info.path << " unexpected non-memory capability" << endl;
  cout << cap << endl;

  return;
}

GCPtr<Value>
pf_doprint_space(PrimFnValue& pfv,
		 InterpState& is,
		 sherpa::CVector<GCPtr<Value> >& args)
{
  GCPtr<Value> v0 = args[0];

  if (v0->kind != Value::vk_cap) {
    // Be forgiving:
    pf_print_one(is, v0);
    std::cout << '\n';
    return &TheUnitValue;
  }

  GCPtr<CapValue> cv = v0.upcast<CapValue>();
  GCPtr<CoyImage> ci = cv->ci;

  const DumpSpaceCBInfo myInfo(std::cout, "space");

  std::cout << "{" << std::endl;
  pf_doprint_space_base(ci, cv->cap, myInfo);
  std::cout << "}" << std::endl;

  return &TheUnitValue;
}
