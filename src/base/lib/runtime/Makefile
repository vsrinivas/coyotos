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

include $(COYOTOS_SRC)/build/make/makevars.mk

CFLAGS+=-g -Os

INC=-I. -I$(BUILDDIR) -I$(COYOTOS_ROOT)/usr/include
SOURCES=$(wildcard *.c) $(wildcard *.S)
C_OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(wildcard c_*.c))
C_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard $(COYOTOS_TARGET)/c_*.c)))
C_OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(notdir $(wildcard $(COYOTOS_TARGET)/c_*.S)))
SMALL_OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(wildcard small_*.c))
SMALL_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard $(COYOTOS_TARGET)/small_*.c)))
SMALL_OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(notdir $(wildcard $(COYOTOS_TARGET)/small_*.S)))
LARGE_OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(wildcard large_*.c))
LARGE_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard $(COYOTOS_TARGET)/large_*.c)))
LARGE_OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(notdir $(wildcard $(COYOTOS_TARGET)/large_*.S)))
COYOTOS_OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(wildcard coyotos_*.c))
COYOTOS_OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(wildcard coyotos_*.S))
OBJECTS=$(C_OBJECTS)

HEADERS=$(RUNTIME_H) $(wildcard coyotos/*.h)

GENRUNTIME=genruntime.sh
PKG_MKIDIR=$(COYOTOS_SRC)/$(PACKAGE)/mki
RUNTIME_MKI=$(PKG_MKIDIR)/coyotos/RunTime.mki
RUNTIME_H=$(BUILDDIR)/coyotos/runtime.h
INC_TARGETS=$(RUNTIME_H)

TARGETS=$(BUILDDIR)/libc-coyotos.a 
TARGETS+=$(BUILDDIR)/libsmall-space.a 
TARGETS+=$(BUILDDIR)/liblarge-space.a 
TARGETS+=$(BUILDDIR)/libcoyotos.a 
TARGETS+=$(BUILDDIR)/libcoyotos-small.a 
TARGETS+=$(BUILDDIR)/libcoyotos-large.a 

VPATH=$(COYOTOS_TARGET):.

ENUM_MODULES=coyotos.TargetInfo
COMMON_MKI_DIR=$(COYOTOS_ROOT)/usr/include/mki
ENUM_HDRS=$(ENUM_MODULES:%=$(BUILDDIR)/%.h)

include $(COYOTOS_SRC)/build/make/makerules.mk

install all: $(TARGETS)

install: all
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/lib
	$(INSTALL) -m 0644 $(TARGETS) $(COYOTOS_ROOT)/usr/lib
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/include/coyotos
	$(INSTALL) -m 0644 $(HEADERS) $(COYOTOS_ROOT)/usr/include/coyotos

$(RUNTIME_H): $(BUILDDIR)/coyotos.RunTime.h $(GENRUNTIME)
	mkdir -p $(BUILDDIR)/coyotos
	bash $(GENRUNTIME) $(RUNTIME_H) $(BUILDDIR)/coyotos.RunTime.h

$(BUILDDIR)/coyotos.RunTime.h: $(RUNTIME_MKI) $(MKIMAGE)
	$(MKIMAGE) -t $(COYOTOS_TARGET) -H $(BUILDDIR) -I $(PKG_MKIDIR) coyotos.RunTime

$(BUILDDIR)/coyotos.TargetInfo.h: $(COMMON_MKI_DIR)/coyotos/TargetInfo/$(COYOTOS_TARGET).mki $(MKIMAGE)
	$(MKIMAGE) -t $(COYOTOS_TARGET) -H $(BUILDDIR) coyotos.TargetInfo

$(OBJECTS): $(ENUM_HDRS) $(RUNTIME_H)

$(BUILDDIR)/libc-coyotos.a: $(C_OBJECTS)
	$(AR) -crv $@ $(C_OBJECTS)

$(BUILDDIR)/liblarge-space.a: $(LARGE_OBJECTS)
	$(AR) -crv $@ $(LARGE_OBJECTS)

$(BUILDDIR)/libsmall-space.a: $(SMALL_OBJECTS)
	$(AR) -crv $@ $(SMALL_OBJECTS)

$(BUILDDIR)/libcoyotos.a: $(COYOTOS_OBJECTS)
	$(AR) -crv $@ $(COYOTOS_OBJECTS)

$(BUILDDIR)/libcoyotos-small.a: $(COYOTOS_OBJECTS) $(SMALL_OBJECTS)
	$(AR) -crv $@ $(COYOTOS_OBJECTS) $(SMALL_OBJECTS)

$(BUILDDIR)/libcoyotos-large.a: $(COYOTOS_OBJECTS) $(LARGE_OBJECTS)
	$(AR) -crv $@ $(COYOTOS_OBJECTS) $(LARGE_OBJECTS)

-include $(BUILDDIR)/.*.m

