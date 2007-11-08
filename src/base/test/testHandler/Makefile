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
TARGETS=$(BUILDDIR)/testHandler $(BUILDDIR)/testProg

HANDLER_OBJECTS = $(BUILDDIR)/testHandler.o $(BUILDDIR)/runtime.o
TESTPROG_OBJECTS = $(BUILDDIR)/testProg.o $(BUILDDIR)/runtime.o

include $(COYOTOS_SRC)/build/make/makerules.mk

IDL_DIR=$(COYOTOS_SRC)/../usr/include/idl/coyotos
IDL_FILES=$(wildcard $(IDL_DIR)/*.idl)

IDL_HEADERS=\
    $(IDL_FILES:$(IDL_DIR)/%.idl=$(BUILDDIR)/idl/coyotos/%.server.h) \

SERVER_INTERFACES=\
    MemoryHandler \
    ProcessHandler

SERVER_IDL_FILES=$(SERVER_INTERFACES:%=$(IDL_DIR)/%.idl)

$(BUILDDIR)/generated-idl: $(IDL_FILES) $(SERVER_IDL_FILES) $(CAPIDL) Makefile
	touch $@
	$(RUN_CAPIDL) -D $(BUILDDIR)/idl -s -I $(IDL_DIR) $(SERVER_IDL_FILES) || (e=$$?; rm -f $@; exit $$e)

$(IDL_HEADERS): $(BUILDDIR)/generated-idl
	@true

ENUM_MODULES=testHandler
COMMON_MKI_DIR=$(COYOTOS_ROOT)/usr/include/mki
ENUM_HDRS=$(ENUM_MODULES:%=$(BUILDDIR)/%.h)

$(BUILDDIR)/testHandler.h: testHandler.mki $(MKIMAGE)
	$(RUN_MKIMAGE) -H $(BUILDDIR) -I . -I $(COMMON_MKI_DIR) testHandler coyotos.TargetInfo

$(OBJECTS): $(BUILDDIR)/generated-idl $(ENUM_HDRS)

install all: $(TARGETS) $(BUILDDIR)/mkimage.out

$(BUILDDIR)/testHandler: $(HANDLER_OBJECTS)
	$(GCC) -small-space $(GPLUSFLAGS) $(HANDLER_OBJECTS) $(LIBS) $(STDLIBDIRS) -o $@

$(BUILDDIR)/testProg: $(TESTPROG_OBJECTS)
	$(GCC) -small-space $(GPLUSFLAGS) $(TESTPROG_OBJECTS) $(LIBS) $(STDLIBDIRS) -o $@

# for test images
$(BUILDDIR)/mkimage.out: $(TARGETS) testHandler.mki $(MKIMAGE)
	$(RUN_MKIMAGE) -o $@ -I. -L$(BUILDDIR) testHandler

-include $(BUILDDIR)/.*.m

