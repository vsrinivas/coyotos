#ifndef ARCHINFO_HXX
#define ARCHINFO_HXX

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

#include <string>

#include <stddef.h>

/** @brief Information that we need to know about each target
    architecture. */
typedef struct ArchInfo {
  const char *name;		// of the architecture
  uint32_t   archID;		// architecture number
  unsigned   endian;		// one of the values in endian.h
  size_t     wordBytes;
  size_t     worstIntAlign;
  size_t     floatByteAlign;
} ArchInfo;

const ArchInfo *findArch(const std::string& nm);

extern const ArchInfo *targetArch;

#endif /* ARCHINFO_HXX */
