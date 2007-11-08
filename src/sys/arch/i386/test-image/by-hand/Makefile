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
default: all
CROSS_BUILD=yes
COYOTOS_SRC=../../../../..

include $(COYOTOS_SRC)/build/make/makevars.mk

CFLAGS += -g

RUN_MKIMAGE=$(COYOTOS_SRC)/ccs/mkimage/$(BUILDDIR)/mkimage

COYQEMU=$(COYOTOS_SRC)/build/bin/i386/coyqemu

IMAGES=$(patsubst %.mki,$(BUILDDIR)/%.img,$(filter-out SimpleProc.mki,$(wildcard *.mki)))
INC=-I$(COYOTOS_SRC)/sys -I$(COYOTOS_SRC)/sys/idl/$(BUILDDIR)/h -IBUILD
LINKOPTS=-Wl,-Ttext,0x1000 -Wl,-Tdata,0x3000

include $(COYOTOS_SRC)/build/make/makerules.mk

install all: $(IMAGES)

.PRECIOUS: $(BUILDDIR)/%.o $(BUILDDIR)/%.exe

#iplimage:
#	$(RUN_MKIMAGE) -t i386 -I. $(IMGFILE)

$(BUILDDIR)/SimpleProc.h: SimpleProc.mki | $(BUILDDIR)
	$(RUN_MKIMAGE) -t i386 -H./$(BUILDDIR) -I. -L. $(patsubst %.mki,%,$<)

$(BUILDDIR)/%.img: %.mki $(BUILDDIR)/%.exe
	$(RUN_MKIMAGE) -t i386 -I. -L$(BUILDDIR) -o $@ $(patsubst $(BUILDDIR)/%.img,%,$@)

$(BUILDDIR)/%.exe: $(BUILDDIR)/%.o $(BUILDDIR)/SimpleProc.h
	$(GCC) -g $(LINKOPTS) $(INC) -nostdlib -o $@ $<

$(BUILDDIR)/%.exe: $(BUILDDIR)/%-data.o $(BUILDDIR)/SimpleProc.h $(BUILDDIR)/do_ipc.o
	$(GCC) -g $(LINKOPTS) $(INC) -nostdlib -o $@ $< $(BUILDDIR)/do_ipc.o

ipc.mki: $(BUILDDIR)/openrcv.exe

# This is complicated by the fact that the .gdb file is optional.  If it
# doesn't exist, we substitute default.gdb
%.run: $(BUILDDIR)/%.img
	@gdb=$(patsubst %.run,%.gdb,$@); \
	 [ -f $$gdb ] || gdb=default.gdb; \
	 cmd="$(COYQEMU) -g -s $$gdb $<"; \
	 echo $$cmd; \
	 exec $$cmd

$(BUILDDIR)/ipc.img: $(BUILDDIR)/openrcv.exe
