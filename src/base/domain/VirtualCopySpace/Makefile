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

INC=-I. -I$(COYOTOS_SRC)/../usr/include -I$(BUILDDIR) -I../../../sys
SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(wildcard *.c))
TARGETS=$(BUILDDIR)/VirtualCopySpace

include $(COYOTOS_SRC)/build/make/makerules.mk

BASE_IDL_DIR=$(PKG_SRC)/idl
BASE_IDL_FILES=$(wildcard $(BASE_IDL_DIR)/[A-Za-z]*.idl)
SYS_IDL_DIR=$(COYOTOS_SRC)/sys/idl
SYS_IDL_FILES=$(wildcard $(SYS_IDL_DIR)/[A-Za-z]*.idl)

SERVER_INTERFACES=\
    VirtualCopySpace \
    MemoryHandler \
    SpaceHandler

SERVER_IDL_FILES=\
    $(BASE_IDL_DIR)/VirtualCopySpace.idl \
    $(SYS_IDL_DIR)/MemoryHandler.idl \
    $(BASE_IDL_DIR)/SpaceHandler.idl

IDL_FILES=\
    $(BASE_IDL_FILES) \
    $(SYS_IDL_FILES)

IDL_HEADERS=\
    $(BASE_IDL_FILES:$(BASE_IDL_DIR)/%.idl=$(BUILDDIR)/idl/coyotos/%.server.h) \
    $(SYS_IDL_FILES:$(SYS_IDL_DIR)/%.idl=$(BUILDDIR)/idl/coyotos/%.server.h)

ENUM_MODULES=coyotos.VirtualCopySpace
BASE_MKI_DIR=$(PKG_SRC)/mki
COMMON_MKI_DIR=$(COYOTOS_ROOT)/usr/include/mki
ENUM_HDRS=$(ENUM_MODULES:%=$(BUILDDIR)/%.h)

$(BUILDDIR)/coyotos.VirtualCopySpace.h: $(BASE_MKI_DIR)/coyotos/VirtualCopySpace.mki $(MKIMAGE)
	$(RUN_MKIMAGE) -H $(BUILDDIR) -I $(BASE_MKI_DIR) -I $(COMMON_MKI_DIR) coyotos.VirtualCopySpace

$(BUILDDIR)/generated-idl: $(IDL_FILES) $(CAPIDL) Makefile
	touch $@
	$(RUN_CAPIDL) -D $(BUILDDIR)/idl -s -I $(BASE_IDL_DIR) -I $(SYS_IDL_DIR) $(SERVER_IDL_FILES) || (e=$$?; rm -f $@; exit $$e)

$(IDL_HEADERS): $(BUILDDIR)/generated-idl
	@true

$(OBJECTS) $(PROTO_OBJECTS): $(BUILDDIR)/generated-idl $(ENUM_HDRS)

install all: $(TARGETS)

install: all
	$(INSTALL) -d $(COYOTOS_ROOT)/usr
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/domain
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/domain/coyotos
	$(INSTALL) -m 0755 $(TARGETS) $(COYOTOS_ROOT)/usr/domain/coyotos

$(BUILDDIR)/%.o: $(COYOTOS_ARCH)/%.S $(MAKE_BUILDDIR)
	$(ASM_DEP)
	$(ASM_BUILD)

$(BUILDDIR)/VirtualCopySpace: $(OBJECTS)
	$(GCC) -small-space $(GPLUSFLAGS) $(OBJECTS) $(LIBS) $(STDLIBDIRS) -o $@

# for test images
$(BUILDDIR)/mkimage.out: install $(TARGETS) $(BASE_MKI_DIR)/coyotos/VirtualCopySpace.mki $(MKIMAGE)
	$(RUN_MKIMAGE) -o $@ -I $(BASE_MKI_DIR) coyotos.VirtualCopySpace

-include $(BUILDDIR)/.*.m

