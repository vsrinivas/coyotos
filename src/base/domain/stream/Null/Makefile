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
COYOTOS_SRC=../../../..
CROSS_BUILD=yes

CFLAGS+=-g -O

INC=-I. -I$(COYOTOS_SRC)/../usr/include -I$(BUILDDIR) -I../../../sys
SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(wildcard *.c))
TARGETS=$(BUILDDIR)/Null

include $(COYOTOS_SRC)/build/make/makerules.mk

ENUM_MODULES=coyotos.stream.Null coyotos.TargetInfo
BASE_MKI_DIR=$(PKG_SRC)/mki
COMMON_MKI_DIR=$(COYOTOS_ROOT)/usr/include/mki
ENUM_HDRS=$(ENUM_MODULES:%=$(BUILDDIR)/%.h)

$(BUILDDIR)/coyotos.stream.Null.h: $(BASE_MKI_DIR)/coyotos/stream/Null.mki $(MKIMAGE)
	$(RUN_MKIMAGE) -H $(BUILDDIR) -I $(BASE_MKI_DIR) -I $(COMMON_MKI_DIR) coyotos.stream.Null

$(BUILDDIR)/coyotos.TargetInfo.h: $(wildcard $(COMMON_MKI_DIR)/coyotos/*/*.mki) $(MKIMAGE)
	$(RUN_MKIMAGE) -H $(BUILDDIR) -I $(BASE_MKI_DIR) -I $(COMMON_MKI_DIR) coyotos.TargetInfo

$(OBJECTS): $(ENUM_HDRS)

install all: $(TARGETS)

install: all
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/domain/coyotos/stream
	$(INSTALL) -m 0755 $(TARGETS) $(COYOTOS_ROOT)/usr/domain/coyotos/stream

$(BUILDDIR)/Null: $(OBJECTS)
	$(GCC) $(GPLUSFLAGS) $(OBJECTS) $(LIBS) $(STDLIBDIRS) -o $@

-include $(BUILDDIR)/.*.m

