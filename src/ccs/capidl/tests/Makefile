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

CROSS_BUILD=yes

COYOTOS_SRC=../../..

include $(COYOTOS_SRC)/build/make/makerules.mk

IDL_DIR=$(COYOTOS_SRC)/sys/idl
IDL_FILES=$(wildcard $(IDL_DIR)/*.idl)
IDL_FILES+=$(wildcard $(IDL_DIR)/$(COYOTOS_ARCH)/*.idl)
IDL_FILES+=$(wildcard *.idl)
RUN_CAPIDL=../$(BUILDDIR)/capidl

OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(wildcard *.c))

INC=-I./BUILD/hdr -I../../../sys


test: $(OBJECTS)

.FORCE:

idl: .FORCE
	make -C ..
	rm -rf idl
	$(RUN_CAPIDL) -D ./BUILD/hdr/idl -a i386 -h -I $(IDL_DIR) $(IDL_FILES) || ( e=$$?; rm -f $@; exit $$e )
	$(RUN_CAPIDL) -D ./BUILD/hdr/idl -a i386 -s -I $(IDL_DIR) $(IDL_FILES) || ( e=$$?; rm -f $@; exit $$e )
	$(RUN_CAPIDL) -a i386 -t -oBUILD/template.c -I $(IDL_DIR) $(IDL_FILES) || ( e=$$?; rm -f $@; exit $$e )

$(OBJECTS): idl

clean-local:
	rm -rf *.s

clean: clean-local
