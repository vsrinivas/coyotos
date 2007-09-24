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
TARGETS=$(BUILDDIR)/TextConsole

include $(COYOTOS_SRC)/build/make/makerules.mk

IDL_DIR=$(COYOTOS_ROOT)/usr/include/idl
MKI_DIR=$(COYOTOS_ROOT)/usr/include/mki
#IDL_DIR+=$(PKG_SRC)/idl

INTERFACES=coyotos.driver.TextConsole
IF_IDL=$(patsubst %,$(IDL_DIR)/%.idl,$(subst .,/,$(INTERFACES)))
IF_HDRS=$(patsubst %,$(BUILDDIR)/idl/%.server.h,$(subst .,/,$(INTERFACES)))

ENUM_MKI=coyotos.TargetInfo.$(COYOTOS_TARGET) coyotos.driver.TextConsole
ENUM_MKI_SRC=$(patsubst %,$(MKI_DIR)/%.mki,$(subst .,/,$(ENUM_MKI)))
ENUM_HDRS=$(patsubst %,$(BUILDDIR)/%.h,$(ENUM_MKI))

echo:
	@echo $(INTERFACES)
	@echo $(IF_IDL)
	@echo $(IF_HDRS)
	@echo $(ENUM_MKI)
	@echo $(ENUM_MKI_SRC)

# Note on make: Using secondary expansion, it is possible in abstract
# to do a minimal rebuild of enum headers, but it hardly seems worth it.

# To generate enum headers from MKI files:
$(ENUM_HDRS): $(ENUM_MKI_SRC) $(MKIMAGE) | $(BUILDDIR)
	$(MKIMAGE) -t $(COYOTOS_TARGET) -H $(BUILDDIR) -I $(MKI_DIR) $(ENUM_MKI)

$(IF_HDRS): $(IF_IDL) $(CAPIDL)
	$(CAPIDL) -D $(BUILDDIR)/idl -a $(COYOTOS_TARGET) -s -I $(IDL_DIR) $(IF_IDL)

check: $(ENUM_HDRS) $(IF_HDRS)

$(OBJECTS): $(ENUM_HDRS) $(IF_HDRS)

install all: $(TARGETS)

install: all
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/domain/coyotos/driver
	$(INSTALL) -m 0755 $(TARGETS) $(COYOTOS_ROOT)/usr/domain/coyotos/driver

$(BUILDDIR)/TextConsole: $(OBJECTS)
	$(GCC) -small-space $(GPLUSFLAGS) $(OBJECTS) $(LIBS) $(STDLIBDIRS) -o $@

$(BUILDDIR)/mkimage.out: $(TARGETS) $(ENUM_MKI_SRC)
	$(MKIMAGE) -o $@ -t $(COYOTOS_TARGET) -I$(MKI_DIR) -L$(BUILDDIR) coyotos.driver.TextConsole

## for test images
#$(BUILDDIR)/mkimage.out: install $(TARGETS) $(BASE_MKI_DIR)/coyotos/SpaceBank.mki
#	$(MKIMAGE) -o $@ -t $(COYOTOS_TARGET) -I $(BASE_MKI_DIR) coyotos.SpaceBank
#
#-include $(BUILDDIR)/.*.m
#
