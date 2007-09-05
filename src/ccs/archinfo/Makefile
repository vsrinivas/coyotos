#
# Copyright (C) 2007, The EROS Group, LLC
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

include $(COYOTOS_SRC)/build/make/makerules.mk

MKI_FILES=$(wildcard mki/coyotos/*.mki mki/coyotos/TargetInfo/*.mki)

RUNTIME_H=$(BUILDDIR)/runtime.h
TARGETS=$(RUNTIME_H)

GENRUNTIME=genruntime.sh

all: $(BUILDDIR) $(TARGETS)

$(RUNTIME_H): $(BUILDDIR)/coyotos.RunTime.h $(GENRUNTIME)
	bash $(GENRUNTIME) $(RUNTIME_H) $(BUILDDIR)/coyotos.RunTime.h

$(BUILDDIR)/coyotos.RunTime.h: mki/coyotos/RunTime.mki
	$(MKIMAGE) -t $(COYOTOS_TARGET) -H $(BUILDDIR) -I mki coyotos.RunTime

install: all
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/include/coyotos
	$(INSTALL) -m 0644 $(BUILDDIR)/runtime.h $(COYOTOS_ROOT)/usr/include/coyotos
	for f in $(MKI_FILES); do \
	  $(INSTALL) -d `dirname $(COYOTOS_ROOT)/usr/include/$$f`; \
	  $(INSTALL) -m 0644 $$f $(COYOTOS_ROOT)/usr/include/$$f; \
	done

-include $(BUILDDIR)/.*.m

