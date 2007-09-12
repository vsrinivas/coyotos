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

#include <elf.h>

#include "CoyImage.hxx"
#include <libsherpa/UExcept.hxx>

#include "../../sys/coyotos/machine/pagesize.h"

using namespace sherpa;

#define DEFARCH(nm, no, nm_upcase, elf_machine, endian)	\
  { #nm, no, COYOTOS_##nm_upcase##_PAGE_SIZE,	\
      COYOTOS_##nm_upcase##_PAGE_ADDR_BITS, elf_machine, endian  },

static ArchInfo archInfo[] = {
#include "arch.def"
  { "", 0, 0, 0, 0 }		// sentinal
};


const ArchInfo& 
CoyImage::findArch(const std::string& nm)
{
  for (size_t i = 0; 
       i < (sizeof(archInfo) / sizeof(archInfo[0]))-1; 
       i++) {
    if (archInfo[i].name == nm)
      return archInfo[i];
  }

  THROW(excpt::BadValue, 
	format("Architecture \"%s\" is unknown.", nm.c_str()));
}

const ArchInfo& 
CoyImage::findArch(uint32_t archNo)
{
  for (size_t i = 0; 
       i < (sizeof(archInfo) / sizeof(archInfo[0]))-1; 
       i++) {
    if (archInfo[i].no == archNo)
      return archInfo[i];
  }

  THROW(excpt::BadValue, 
	format("Architecture %d is unknown.", archNo));
}
