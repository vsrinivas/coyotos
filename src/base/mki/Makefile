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

MKISRC=$(wildcard */[A-Za-z]*.mki)
MKISRC+=$(wildcard */*/[A-Za-z]*.mki)

include $(COYOTOS_SRC)/build/make/makerules.mk

install: $(MKISRC)
	for mki in $(MKISRC); do\
		$(INSTALL) -d $(COYOTOS_ROOT)/usr/include/mki/`dirname $$mki`; \
		$(INSTALL) -m 0644 $$mki $(COYOTOS_ROOT)/usr/include/mki/`dirname $$mki`; \
	done
