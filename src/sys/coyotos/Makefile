#
# Copyright (C) 2005, 2006, The EROS Group, LLC.
#
# This file is part of the COYOTOS Operating System runtime library.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
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
DIRS=machine
DIRS+=i386
DIRS+=coldfire

HEADERS=\
	capidl.h \
	coytypes.h \
	endian.h \
	syscall.h

#	cap-instr.h \
#	Invoke.h \
#	NodeKey.h \
#	PageKey.h \
#	DevCreKey.h \
#	DeviceKey.h \
#	KeyboardKey.h \
#	ConsoleKey.h \
#	SchedCreKey.h \
#	Device.h \
#	Reserve.h \
#	DeviceDef.h \
#	KeyConst.h \
#	SegmentKey.h \
#	SegKeeperInfo.h \
#	endian.h \
#	stdarg.h \
#	TimeOfDay.h \
#	TimePage.h \
#	Registers.h \
#	ProcStats.h \
#	CharSrcProto.h \
#	TtyKey.h \
#	ProcessKey.h \
#	ProcessState.h

IDLSRC=$(wildcard *.idl)

#GENERATED=StdKeyType.h

include $(COYOTOS_SRC)/build/make/makerules.mk

GENKEYTYPES=../scripts/GenKeyTypes.awk
KEYDEFS=../KEYDEFS

#.FORCE:
#
#StdKeyType.h: $(KEYDEFS)  .FORCE
#	$(COYOTOS_CPP) $(KEYDEFS) | $(GAWK) -f $(GENKEYTYPES) > $@.tmp
#	$(REPLACE) $@.tmp $@

install: $(GENERATED)
	$(INSTALL) -d $(COYOTOS_ROOT)/usr
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/include
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/include/coyotos
	$(INSTALL) -m 0644 $(HEADERS) $(GENERATED) $(COYOTOS_ROOT)/usr/include/coyotos

#interfaces:
#	$(INSTALL) -d $(COYOTOS_ROOT)/idl
#	$(INSTALL) -d $(COYOTOS_ROOT)/idl/coyotos
#	$(INSTALL) -m 0644 $(IDLSRC) $(COYOTOS_ROOT)/idl/coyotos

.PRECIOUS: $(HEADERS)
