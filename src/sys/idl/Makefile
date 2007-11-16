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
include $(COYOTOS_SRC)/build/make/makevars.mk

TARGETS=
# Need to install headers for all targets, not just for current one,
# so visit all of the architecture-dependent subdirectories for now.
# There is a better solution that can be used for install, but 
#DIRS=machine
#DIRS+=i386
#DIRS+=coldfire

IDL=$(wildcard [A-Za-z]*.idl)
IDL+=$(wildcard */[A-Za-z]*.idl)
IDLPKG=coyotos
IDLDIR=$(subst .,/,$(IDLPKG))
HDR_NAMES=$(patsubst %.idl,%.h,$(IDL))
HDR_NAMES+=$(patsubst %.idl,%.server.h,$(IDL))
IDLHDR=$(patsubst %.idl,$(BUILDDIR)/idl/$(IDLDIR)/%.h,$(IDL))
IDLSHDR=$(patsubst %.idl,$(BUILDDIR)/idl/$(IDLDIR)/%.server.h,$(IDL))
IDLDEPS=$(patsubst %.idl,$(BUILDDIR)/.$(subst /,.,%.m),$(IDL))

# This is moderately sleazy. The following matches all of the stub
# source files, but only if they have been generated already. We will
# need to invoke this makefile recursively to make use of this.
INC=-I$(BUILDDIR) -I$(COYOTOS_ROOT)/usr/include
IDLSTUBS=$(wildcard $(BUILDDIR)/stub/*.c)
OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(IDLSTUBS)))
VPATH=$(BUILDDIR)/stub

include $(COYOTOS_SRC)/build/make/makerules.mk

# This is a great lot of unnecessary work, because all of these
# headers could happily be built simultaneously, but @)&%)(@$& make is
# unable to describe multiple targets! It's probably better this way,
# because it reduces consequential rebuild.

$(IDLHDR): $(BUILDDIR)/idl/$(IDLDIR)/%.h : %.idl $(CAPIDL) | $(BUILDDIR) 
	@$(RUN_CAPIDL) -I. -D $(BUILDDIR)/idl -l c-header-depend -o $(BUILDDIR)/.$(subst /,.,$<.m) $<
	$(RUN_CAPIDL) -I. -D $(BUILDDIR)/idl -h $<

$(IDLSHDR): $(BUILDDIR)/idl/$(IDLDIR)/%.server.h : %.idl $(CAPIDL) | $(BUILDDIR) 
	@$(RUN_CAPIDL) -I. -D $(BUILDDIR)/idl -l c-server-header-depend -o $(BUILDDIR)/.$(subst /,.,$<.m) $<
	$(RUN_CAPIDL) -I. -D $(BUILDDIR)/idl -s $<

interfaces: $(IDL) $(IDLHDR) $(IDLSHDR)
	for idlfile in $(IDL); do\
		$(INSTALL) -d $(COYOTOS_ROOT)/usr/include/idl/$(IDLDIR)/`dirname $$idlfile`; \
		$(INSTALL) -m 0644 $$idlfile $(COYOTOS_ROOT)/usr/include/idl/$(IDLDIR)/`dirname $$idlfile`; \
	done
	for hdr in $(HDR_NAMES); do\
		$(INSTALL) -d $(COYOTOS_ROOT)/usr/include/idl/$(IDLDIR)/`dirname $$hdr`; \
		$(INSTALL) -m 0644 $(BUILDDIR)/idl/$(IDLDIR)/$$hdr $(COYOTOS_ROOT)/usr/include/idl/$(IDLDIR)/`dirname $$hdr`; \
	done

# The following are testing targets:
idl-h: $(IDLHDR) $(IDLSHDR)

idl-stubs: $(IDL) $(CAPIDL)
	$(RUN_CAPIDL) -I. -D $(BUILDDIR)/stub -l c-stub $(IDL)


#$(BUILDDIR)/.stubs.m: $(IDL) | $(BUILDDIR)
#	@$(RUN_CAPIDL) -I. -D $(BUILDDIR) -X $(BUILDDIR) -l c-stub-depend -A $(TARGETS) $(IDL) -o $@

libidl-sys: $(OBJECTS)
	$(COYOTOS_AR) crv $@ $(OBJECTS)
	$(COYOTOS_RANLIB) $@

$(BUILDDIR)/libidl-sys.a: $(IDL) $(IDLHDR) $(CAPIDL)
	$(RUN_CAPIDL) -I. -D $(BUILDDIR)/stub -l c-stub $(IDL)
	$(MAKE) libidl-sys

#libs: idl-stubs

$(BUILDDIR)/%.c: %.idl
	echo "BUILD $@ from $<"

.PRECIOUS: $(HEADERS)

# This is very sleazy. Because this include is not prefixed by '-', it
# has the effect of forcing make to attempt to generate the stub
# dependencies file if it is not present or if it is out of date
# BEFORE processing other rules. This in turn has the effect of
# dynamically constructing the correct dependencies for libidl-sys.a
#-include $(BUILDDIR)/.stubs.m
-include $(IDLDEPS)
