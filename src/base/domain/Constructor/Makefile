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
PROTO_OBJECTS=\
    $(patsubst $(COYOTOS_TARGET)/%.S,$(BUILDDIR)/%.o,$(wildcard $(COYOTOS_TARGET)/*.S)) \
    $(patsubst $(COYOTOS_TARGET)/%.c,$(BUILDDIR)/%.o,$(wildcard $(COYOTOS_TARGET)/*.c))
TARGETS=$(BUILDDIR)/Constructor $(BUILDDIR)/ProtoSpace

include $(COYOTOS_SRC)/build/make/makerules.mk

BASE_IDL_DIR=$(PKG_SRC)/idl
BASE_IDL_FILES=$(wildcard $(BASE_IDL_DIR)/*.idl)
SYS_IDL_DIR=$(COYOTOS_SRC)/sys/idl
SYS_IDL_FILES=$(wildcard $(SYS_IDL_DIR)/*.idl)

SERVER_INTERFACES=\
    Verifier \
    Constructor \
    Builder

SERVER_IDL_FILES=$(SERVER_INTERFACES:%=$(BASE_IDL_DIR)/%.idl)

IDL_FILES=\
    $(BASE_IDL_FILES) \
    $(SYS_IDL_FILES)

IDL_HEADERS=\
    $(BASE_IDL_FILES:$(BASE_IDL_DIR)/%.idl=$(BUILDDIR)/idl/coyotos/%.server.h) \
    $(SYS_IDL_FILES:$(SYS_IDL_DIR)/%.idl=$(BUILDDIR)/idl/coyotos/%.server.h)

ENUM_MODULES=coyotos.Constructor
BASE_MKI_DIR=$(PKG_SRC)/mki
COMMON_MKI_DIR=$(COYOTOS_ROOT)/usr/include/mki
ENUM_HDRS=$(ENUM_MODULES:%=$(BUILDDIR)/%.h)

$(BUILDDIR)/coyotos.Constructor.h: $(BASE_MKI_DIR)/coyotos/Constructor.mki $(MKIMAGE)
	$(MKIMAGE) -t $(COYOTOS_TARGET) -H $(BUILDDIR) -I $(BASE_MKI_DIR) -I $(COMMON_MKI_DIR) coyotos.Constructor

$(BUILDDIR)/generated-idl: $(IDL_FILES) $(SERVER_IDL_FILES) $(CAPIDL) Makefile
	touch $@
	$(CAPIDL) -D $(BUILDDIR)/idl -a $(COYOTOS_TARGET) -s -I $(BASE_IDL_DIR) -I $(SYS_IDL_DIR) $(SERVER_IDL_FILES) || (e=$$?; rm -f $@; exit $$e)

$(IDL_HEADERS): $(BUILDDIR)/generated-idl
	@true

$(OBJECTS) $(PROTO_OBJECTS): $(BUILDDIR)/generated-idl $(ENUM_HDRS)

install all: $(TARGETS)

install: all
	$(INSTALL) -d $(COYOTOS_ROOT)/usr
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/domain
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/domain/coyotos
	$(INSTALL) -m 0755 $(TARGETS) $(COYOTOS_ROOT)/usr/domain/coyotos

$(BUILDDIR)/%.o: $(COYOTOS_TARGET)/%.S $(MAKE_BUILDDIR)
	$(ASM_DEP)
	$(ASM_BUILD)

$(BUILDDIR)/%.o: $(COYOTOS_TARGET)/%.c $(MAKE_BUILDDIR)
	$(C_DEP)
	$(C_BUILD)

$(BUILDDIR)/Constructor: $(OBJECTS)
	$(GCC) -small-space $(GPLUSFLAGS) $(OBJECTS) $(LIBS) $(STDLIBDIRS) -o $@

$(BUILDDIR)/ProtoSpace: $(PROTO_OBJECTS)
	$(LD) -Ttext 0x0 -o $@ $(PROTO_OBJECTS)

# for test images
$(BUILDDIR)/mkimage.out: install $(TARGETS) $(BASE_MKI_DIR)/coyotos/Constructor.mki
	$(MKIMAGE) -o $@ -t $(COYOTOS_TARGET) -I $(BASE_MKI_DIR) coyotos.Constructor

-include $(BUILDDIR)/.*.m

