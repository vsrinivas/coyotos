#
# Copyright (C) 2007, The EROS Group, LLC.
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
COYOTOS_SRC=../../..
CROSS_BUILD=yes

CFLAGS+=-g -O

INC=-I. -I$(COYOTOS_SRC)/../usr/include -I$(BUILDDIR)
SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(wildcard *.c))
TARGETS=$(BUILDDIR)/testConstructor

include $(COYOTOS_SRC)/build/make/makerules.mk

ENUM_MODULES=testConstructor
COMMON_MKI_DIR=$(COYOTOS_ROOT)/usr/include/mki
ENUM_HDRS=$(ENUM_MODULES:%=$(BUILDDIR)/%.h)

$(BUILDDIR)/testConstructor.h: testConstructor.mki $(MKIMAGE)
	$(RUN_MKIMAGE) -H $(BUILDDIR) -I . -I $(COMMON_MKI_DIR) testConstructor

$(OBJECTS): $(ENUM_HDRS)

install all: $(TARGETS) $(BUILDDIR)/mkimage.out

$(BUILDDIR)/testConstructor: $(OBJECTS)
	$(GCC) -small-space $(GPLUSFLAGS) $(OBJECTS) $(LIBS) $(STDLIBDIRS) -o $@

# for test images
$(BUILDDIR)/mkimage.out: $(TARGETS) testConstructor.mki $(MKIMAGE)
	$(RUN_MKIMAGE) -o $@ -I. -L$(BUILDDIR) testConstructor

-include $(BUILDDIR)/.*.m

