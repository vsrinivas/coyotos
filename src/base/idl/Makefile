#
# Copyright (C) 2007, The EROS Group, LLC.
#
# This file is part of the COYOTOS Operating System runtime library.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with this library; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
#
default: package

COYOTOS_SRC=../..

IDLSRC=$(wildcard [A-Za-z]*.idl)
IDLSRC+=$(wildcard */[A-Za-z]*.idl)
IDLPKG=coyotos
IDLDIR=$(subst ".","/",$(IDLPKG))
IDLHDR=$(patsubst %.idl,$(BUILDDIR)/h/$(IDLDIR)/%.h,$(IDLSRC))
HDR_NAMES=$(patsubst %.idl,%.h,$(IDLSRC))
IDLINC=-I. -I$(COYOTOS_ROOT)/usr/include/idl

include $(COYOTOS_SRC)/build/make/makerules.mk

$(IDLHDR): $(BUILDDIR)/h/$(IDLDIR)/%.h : %.idl $(CAPIDL) | $(BUILDDIR)
	@$(RUN_CAPIDL) $(IDLINC) -D $(BUILDDIR)/h -l c-header-depend -o $(BUILDDIR)/.$(subst /,.,$<.m) $<
	$(RUN_CAPIDL) $(IDLINC) -D $(BUILDDIR)/h -h $<

interfaces: $(IDLSRC) $(IDLHDR)
	for idlfile in $(IDLSRC); do\
		$(INSTALL) -d $(COYOTOS_ROOT)/usr/include/idl/$(IDLDIR)/`dirname $$idlfile`; \
		$(INSTALL) -m 0644 $$idlfile $(COYOTOS_ROOT)/usr/include/idl/$(IDLDIR)/`dirname $$idlfile`; \
	done
	for hdr in $(HDR_NAMES); do\
		$(INSTALL) -d $(COYOTOS_ROOT)/usr/include/idl/$(IDLDIR)/`dirname $$hdr`; \
		$(INSTALL) -m 0644 $(BUILDDIR)/h/$(IDLDIR)/$$hdr $(COYOTOS_ROOT)/usr/include/idl/$(IDLDIR)/`dirname $$hdr`; \
	done

.PRECIOUS: $(HEADERS)
