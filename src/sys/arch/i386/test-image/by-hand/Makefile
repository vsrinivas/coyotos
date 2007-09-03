#
# Copyright (C) 2007, Jonathan S. Shapiro.
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
CROSS_COMPILE=yes
COYOTOS_SRC=../../../../..

include $(COYOTOS_SRC)/build/make/makevars.mk

MKIMAGE=$(COYOTOS_SRC)/ccs/mkimage/$(BUILDDIR)/mkimage
KERNEL=../../kernel/$(BUILDDIR)/coyotos

IMAGES=$(filter-out SimpleProc,$(patsubst %.mki,%,$(wildcard *.mki)))
INC=-I$(COYOTOS_SRC)/sys -I$(COYOTOS_SRC)/sys/idl/$(BUILDDIR)/h -IBUILD
LINKOPTS=-Wl,-Ttext,0x1000 -Wl,-Tdata,0x3000
QEMU=qemu

CLEANLIST=*.exe *.img do_ipc.o

include $(COYOTOS_SRC)/build/make/makerules.mk

#iplimage:
#	$(MKIMAGE) -t i386 -I. $(IMGFILE)

BUILD/SimpleProc.h: SimpleProc.mki
	$(MKIMAGE) -t i386 -H./BUILD -I. -L. $(patsubst %.mki,%,$<)

%.img: %.mki %.exe
	$(MKIMAGE) -t i386 -I. -L. -o $@ $(patsubst %.img,%,$@)

%.exe: %.S BUILD/SimpleProc.h
	$(GCC) -g $(LINKOPTS) $(INC) -nostdlib -o $@ $<

do_ipc.o: do_ipc.S
	$(GCC) -g $(INC) -c -o $@ $<

%.exe: %-data.c BUILD/SimpleProc.h do_ipc.o
	$(GCC) -g $(LINKOPTS) $(INC) -nostdlib -o $@ $< do_ipc.o

%.run: %.img
	cp ../grubdisk .
	mcopy -Do -i grubdisk $< ::mkimage.out
	mcopy -Do -i grubdisk $(KERNEL) ::
	sh ./testme.sh $(patsubst %.run,%,$@)

ipc.img: openrcv.exe
