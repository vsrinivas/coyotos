#
# Copyright (C) 2007, The EROS Group, LLC.
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

COYOTOS_SRC=../../..
include $(COYOTOS_SRC)/build/make/makevars.mk

BUILDDIR=.
TARGETS=
DIRS=

HEADERS=$(wildcard *.h)

include $(COYOTOS_SRC)/build/make/makerules.mk

all:

# NOTE that target-asm.h also gets installed in the domain include tree!
install: $(HEADERS)
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/include
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/include/coyotos
	$(INSTALL) -d $(COYOTOS_ROOT)/usr/include/coyotos/i386
	$(INSTALL) -m 0644 $(HEADERS) $(COYOTOS_ROOT)/usr/include/coyotos/i386

.PRECIOUS: $(LOCAL_HEADERS)
