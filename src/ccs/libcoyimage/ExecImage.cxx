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

#include <iostream>
#include <fstream>
#include <iomanip>

#include <libsherpa/iBinaryStream.hxx>
#include <libsherpa/UExcept.hxx>
#include <elf.h>
#include <a.out.h>

using namespace sherpa;
using namespace std;

#include "ExecImage.hxx"

// #define DEBUG

#ifdef DEBUG
#define PT_LOOS		0x60000000	/* Start of OS-specific */
#define PT_GNU_EH_FRAME	0x6474e550	/* GCC .eh_frame_hdr segment */
#define PT_GNU_STACK	0x6474e551	/* Indicates stack executability */
#define PT_GNU_RELRO	0x6474e552	/* Read-only after relocation */
#define PT_LOSUNW	0x6ffffffa
#define PT_SUNWBSS	0x6ffffffa	/* Sun Specific segment */
#define PT_SUNWSTACK	0x6ffffffb	/* Stack segment */
#define PT_HISUNW	0x6fffffff
#define PT_HIOS		0x6fffffff	/* End of OS-specific */
#define PT_LOPROC	0x70000000	/* Start of processor-specific */
#define PT_HIPROC	0x7fffffff	/* End of processor-specific */

#define DEFPHTYPE(id) { id, #id },
struct PhTypeName {
  unsigned long pt;
  const char *name;
} ph_type_names[] = {
  { PT_NULL,         "NULL" },
  { PT_LOAD,         "LOAD" },
  { PT_DYNAMIC,      "DYNAMIC" },
  { PT_INTERP,       "INTERP" },
  { PT_NOTE,         "NOTE" },
  { PT_SHLIB,        "SHLIB" },
  { PT_PHDR,         "PHDR" },
  { PT_TLS,          "TLS" },
  { PT_LOOS,         "LOOS" },
  { PT_GNU_EH_FRAME, "GNU_EH_FRAME" }, // GCC .eh_frame_hdr
  { PT_GNU_STACK,    "GNU_STACK" }, // Stack executability
  { PT_GNU_RELRO,    "GNU_RELRO" }, // Read-only after relocation
  { PT_LOSUNW,       "LOSUNW" },
  { PT_SUNWBSS,      "SUNWBSS" }, // SUN-specific
  { PT_SUNWSTACK ,   "SUNWSTACK" }, // stack segment
  { PT_HISUNW,       "HISUNW" },
  { PT_HIOS,         "HIOS" },
  { PT_LOPROC,       "LOPROC" },
  { PT_HIPROC,       "HIPROC" },

  { 0, 0 }			// end marker
};

static const char *
getPhTypeName(uint32_t phType)
{
  for (PhTypeName *phtn = ph_type_names; phtn->name; phtn++) {
    if (phtn->pt == phType)
      return phtn->name;
  }

  return "??UNKNOWN??";
}
#endif

bool
ExecRegion::vAddrOverlaps(uint64_t base, uint64_t bound)
{
  if (bound <= vaddr)
    return false;

  uint64_t myBound = vaddr + memsz;
  if (base >= myBound)
    return false;

  // Else whole or partial overlap
  return true;
}; 

bool
ExecRegion::fileOffsetOverlaps(uint64_t base, uint64_t bound)
{
  if (bound <= offset)
    return false;

  uint64_t myBound = offset + filesz;
  if (base >= myBound)
    return false;

  // Else whole or partial overlap
  return true;
}; 

static bool
offset_ascending(const ExecRegion& er1, const ExecRegion& er2)
{
  return er1.offset < er2.offset;
}

static bool
vaddr_ascending(const ExecRegion& er1, const ExecRegion& er2)
{
  return er1.vaddr < er2.vaddr;
}

void
ExecImage::sortByOffset()
{
  // We want them sorted by file offset:
  std::sort(region.begin(), region.end(), offset_ascending);
}

void
ExecImage::sortByAddress()
{
  // We want them sorted by file offset:
  std::sort(region.begin(), region.end(), vaddr_ascending);
}

void
ExecImage::InitElf32(iBinaryStream& ibs)
{
  Elf32_Ehdr ehdr;

  imageTypeName == "ELF32";

  // Use IBS to read the rest of the ELF header so we get it properly byte
  // swapped in the cross-architecture case:
  if (!ibs.seekg(0))
    THROW(excpt::IoError, 
	  format("File \"%s\" failed to seek", name.c_str()));

  ibs.read(EI_NIDENT, ehdr.e_ident);

  ibs >> ehdr.e_type
      >> ehdr.e_machine
      >> ehdr.e_version
      >> ehdr.e_entry
      >> ehdr.e_phoff
      >> ehdr.e_shoff
      >> ehdr.e_flags
      >> ehdr.e_ehsize
      >> ehdr.e_phentsize
      >> ehdr.e_phnum
      >> ehdr.e_shentsize
      >> ehdr.e_shnum
      >> ehdr.e_shstrndx;

  entryPoint = ehdr.e_entry;
  elfMachine = ehdr.e_machine;

  for (size_t i = 0; i < ehdr.e_phnum; i++) {
    if (!ibs.seekg(ehdr.e_phoff + ehdr.e_phentsize * i))
      THROW(excpt::IoError, 
	    format("File \"%s\" failed to seek", name.c_str()));

    Elf32_Phdr phdr;

    // Use IBS to read the program header so we get them properly byte
    // swapped in the cross-architecture case:
    ibs >> phdr.p_type
	>> phdr.p_offset
	>> phdr.p_vaddr
	>> phdr.p_paddr
	>> phdr.p_filesz
	>> phdr.p_memsz
	>> phdr.p_flags
	>> phdr.p_align;

#ifdef DEBUG
    {
      char perm[4] = "\0\0\0";
      char *pbuf = perm;

      if (phdr.p_flags & PF_R)
	*pbuf++ = 'R';
      if (phdr.p_flags & PF_W)
	*pbuf++ = 'W';
      if (phdr.p_flags & PF_X)
	*pbuf++ = 'X';

    
      std::cerr << "phdr["
		<< getPhTypeName(phdr.p_type)
		<<"] "
		<< perm 
		<< std::showbase
		<< std::hex
		<< " va="
		<< phdr.p_vaddr
		<< " memsz="
		<< phdr.p_memsz
		<< " filesz="
		<< phdr.p_filesz
		<< " offset="
		<< phdr.p_offset
		<< std::dec
		<< std::endl;
    }
#endif

    if (phdr.p_type == PT_LOAD) {
      ExecRegion er;

      er.vaddr  = phdr.p_vaddr;
      er.memsz  = phdr.p_memsz;
      er.filesz = phdr.p_filesz;
      er.offset = phdr.p_offset;
      er.perm   = phdr.p_flags & 0xfu;

      region.push_back(er);
    }
  }
}

void
ExecImage::InitElf64(iBinaryStream& ibs)
{
  Elf64_Ehdr ehdr;

  imageTypeName == "ELF32";

  // Use IBS to read the rest of the ELF header so we get it properly byte
  // swapped in the cross-architecture case:
  if (!ibs.seekg(0))
    THROW(excpt::IoError, 
	  format("File \"%s\" failed to seek", name.c_str()));

  ibs.read(EI_NIDENT, ehdr.e_ident);

  ibs >> ehdr.e_type
      >> ehdr.e_machine
      >> ehdr.e_version
      >> ehdr.e_entry
      >> ehdr.e_phoff
      >> ehdr.e_shoff
      >> ehdr.e_flags
      >> ehdr.e_ehsize
      >> ehdr.e_phentsize
      >> ehdr.e_phnum
      >> ehdr.e_shentsize
      >> ehdr.e_shnum
      >> ehdr.e_shstrndx;

  entryPoint = ehdr.e_entry;
  elfMachine = ehdr.e_machine;

  for (size_t i = 0; i < ehdr.e_phnum; i++) {
    if (!ibs.seekg(ehdr.e_phoff + ehdr.e_phentsize * i))
      THROW(excpt::IoError, 
	    format("File \"%s\" failed to seek", name.c_str()));

    Elf64_Phdr phdr;

    // Use IBS to read the program header so we get them properly byte
    // swapped in the cross-architecture case:
    ibs >> phdr.p_type
	>> phdr.p_offset
	>> phdr.p_vaddr
	>> phdr.p_paddr
	>> phdr.p_filesz
	>> phdr.p_memsz
	>> phdr.p_flags
	>> phdr.p_align;

    if (phdr.p_type == PT_LOAD) {
      ExecRegion er;

      er.vaddr  = phdr.p_vaddr;
      er.memsz  = phdr.p_memsz;
      er.filesz = phdr.p_filesz;
      er.offset = phdr.p_offset;
      er.perm   = phdr.p_flags & 0xfu;

      region.push_back(er);
    }
  }
}

ExecImage::ExecImage(const std::string& imageFileName)
{
  name = imageFileName;

  Path ifPath(imageFileName);
  if (!ifPath.exists())
    THROW(excpt::BadValue, 
	  format("File \"%s\" not found", imageFileName.c_str()));

  Path::portstat_t ps = ifPath.stat();

  fstream ifs(imageFileName.c_str(), ios_base::binary|ios_base::in);

  if (!ifs.is_open())
    THROW(excpt::NoObject, 
	  format("File \"%s\" cannot be opened", imageFileName.c_str()));

  iBinaryStream ibs(ifs);

  // Slurp the entire image:
  ibs.read(ps.len, image);

  // Try initializing the various file types that we know about:
  char elfIdent[EI_NIDENT];
  memcpy(elfIdent, image.c_str(), EI_NIDENT);

  // We no longer support a.out formats
  if(memcmp(elfIdent, "\177ELF", SELFMAG) != 0)
    THROW(excpt::BadValue, "Unknown binary image type.");

  switch(elfIdent[EI_DATA]) {
  case ELFDATA2LSB:
    ibs >> bs_base::LittleEndian;
    break;
  case ELFDATA2MSB:
    ibs >> bs_base::BigEndian;
    break;
  }

  if (elfIdent[EI_CLASS] == ELFCLASS32)
    InitElf32(ibs);
  else if (elfIdent[EI_CLASS] == ELFCLASS64)
    InitElf64(ibs);
  else
    THROW(excpt::BadValue, "Unknown ELF class");
}
