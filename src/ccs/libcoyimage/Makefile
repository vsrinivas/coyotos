# Copyright (C) 2007, The EROS Group, LLC
# All rights reserved.
#
# This file is part of the Coyotos Operating System.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

default: package
COYOTOS_SRC=../..

CXXFLAGS=-g

INC=-I. -I.. -I$(COYOTOS_SRC)/sys $(INC_SHERPA)
DEF+=-DCROSS_COMPILING -DCOYOTOS_UNIVERSAL_CROSS

include $(COYOTOS_SRC)/build/make/makerules.mk

SOURCES=$(wildcard *.cxx)
OBJECTS=$(patsubst %.cxx,$(BUILDDIR)/%.o,$(SOURCES))

all: $(BUILDDIR)/libcoyimage.a
install: all

#This is a placeholder.

$(BUILDDIR)/libcoyimage.a: $(OBJECTS)
	$(AR) -crv $@ $(OBJECTS)
	$(RANLIB) $@

arch.def: $(COYOTOS_SRC)/sys/ARCH-LIST GenArchInfo.awk
	$(GAWK) -f GenArchInfo.awk $@ < $(COYOTOS_SRC)/sys/ARCH-LIST

$(BUILDDIR)/CoyImageArch.o: CoyImageArch.cxx arch.def

-include $(BUILDDIR)/.*.m

