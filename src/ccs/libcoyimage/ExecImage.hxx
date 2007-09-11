#ifndef LIBCOYIMAGE_EXECIMAGE_HXX
#define LIBCOYIMAGE_EXECIMAGE_HXX
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

#include <vector>
#include <coyotos/coytypes.h>
#include <libsherpa/GCPtr.hxx>
#include <libsherpa/Path.hxx>
#include <libsherpa/ByteString.hxx>
#include <libsherpa/ifBinaryStream.hxx>

/* ExecRegion types. Same as ELF, which is not by accident! */
enum {
  ER_X = 0x1,
  ER_W = 0x2,
  ER_R = 0x4,
};

/** @brief Data structure capturing a loadable region of an executable
 * image.
 *
 * If the ExecRegion structure looks suspiciously similar to the
 * equivalent structure in the ELF binary format, that's because it
 * is. Since I helped design ELF, this is no surprise.  The heart of
 * the problem here is that the amount of flexibility inherent in ELF
 * is considerable, so no lesser way of capturing the binary image
 * will suffice in ExecImage.  The good news is that we need only pay
 * attention to the program headers - the section headers are for our
 * purposes irrelevant.
 */
struct ExecRegion {
  uint64_t vaddr;
  uint64_t memsz;
  uint64_t filesz;
  uint64_t offset;
  uint64_t perm;

  bool vAddrOverlaps(uint64_t base, uint64_t bound);
  bool fileOffsetOverlaps(uint64_t base, uint64_t bound);
}; 

/** Data structure for managing an executable image. */
class ExecImage : public sherpa::Countable {
  void InitElf32(sherpa::ifBinaryStream&);
  void InitElf64(sherpa::ifBinaryStream&);

public:
  std::string imageTypeName;
  std::string name;
  uint16_t    elfMachine;

  sherpa::ByteString image;

  std::vector<ExecRegion> region;
  
  coyaddr_t  entryPoint;

  // uint64_t GetSymbolValue(const std::string& symName);

  ExecImage(const std::string& imageFileName);

  void sortByOffset();
  void sortByAddress();
};
#endif /* LIBCOYIMAGE_EXECIMAGE_HXX */
