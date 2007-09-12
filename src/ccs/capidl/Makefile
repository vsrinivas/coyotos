#
# Copyright (C) 2001, 2001, The EROS Group, LLC.
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

include $(COYOTOS_SRC)/build/make/makevars.mk

INC=-Ifrontend -I.. -I../../sys $(INC_SHERPA)

TARGETS=$(BUILDDIR)/capidl
CLEANLIST=idl.c idl.h idl.output idl.c.h idl.c.output
CLEANLIST+= y.tab.c
CLEANLIST+= PrescanLexer.c
#OPTIM=-O
OPTIM=-g

OBJECTS= $(BUILDDIR)/idl.o
OBJECTS += $(BUILDDIR)/LTokString.o
OBJECTS += $(BUILDDIR)/IdlLexer.o
OBJECTS += $(BUILDDIR)/SymTab.o
OBJECTS += $(BUILDDIR)/DomNode.o
OBJECTS += $(BUILDDIR)/UCSLexer.o
OBJECTS += $(BUILDDIR)/PrescanLexer.o
OBJECTS += $(BUILDDIR)/ArchInfo.o
OBJECTS += $(BUILDDIR)/capidl.o
OBJECTS += $(BUILDDIR)/util.o
OBJECTS += $(BUILDDIR)/value.o
OBJECTS += $(BUILDDIR)/output.o
OBJECTS += $(BUILDDIR)/rewrite_c.o
OBJECTS += $(BUILDDIR)/DocComment.o
OBJECTS += $(BUILDDIR)/o_symdump.o
OBJECTS += $(BUILDDIR)/o_xmldoc.o
OBJECTS += $(BUILDDIR)/o_html.o
OBJECTS += $(BUILDDIR)/o_c_hdr.o
OBJECTS += $(BUILDDIR)/o_c_hdr_depend.o
#OBJECTS += $(BUILDDIR)/o_c_stubs.o
#OBJECTS += $(BUILDDIR)/o_c_server.o
#OBJECTS += $(BUILDDIR)/o_c_server_hdr.o
#OBJECTS += $(BUILDDIR)/o_c_stub_depend.o
OBJECTS += $(BUILDDIR)/o_capidl.o
OBJECTS += $(BUILDDIR)/o_depend.o

VPATH=frontend:backend

include $(COYOTOS_SRC)/build/make/makerules.mk

OTHER_LIBS=	-lbfd -liberty -lstdc++ -lssl $(LIBSHERPA)
OTHER_LIBS+=-lgc
OTHER_LIBS+=-lgccpp
OTHER_LIBS+=-lcrypto
OTHER_LIBS+=-licuuc

all: $(TARGETS) 

$(BUILDDIR)/capidl: $(OBJECTS)
	$(GPLUS) $(GPLUSFLAGS) -o $@ $(OBJECTS) $(XENV_LIBDIR) $(OTHER_LIBS)

$(BUILDDIR)/idl.o: $(BUILDDIR)/idl.cxx
	$(CXX_DEP)
	$(CXX_BUILD) 

$(BUILDDIR)/idl.cxx $(BUILDDIR)/idl.hxx: idl.y $(MAKE_BUILDDIR)
	bison -t -p idl -v -d -o $(BUILDDIR)/idl.cxx -y $<

$(BUILDDIR)/prescanlexer.o: prescanlexer.cxx $(BUILDDIR)/idl.hxx

arch.def: $(COYOTOS_SRC)/sys/ARCH-LIST GenArchInfo.awk
	$(GAWK) -f GenArchInfo.awk $@ < $(COYOTOS_SRC)/sys/ARCH-LIST

$(BUILDDIR)/ArchInfo.o: ArchInfo.cxx arch.def

install: all
	$(INSTALL) -d $(COYOTOS_ROOT)/host
	$(INSTALL) -d $(COYOTOS_ROOT)/host/bin
	$(INSTALL) -m 0755 $(TARGETS) $(COYOTOS_ROOT)/host/bin

-include $(BUILDDIR)/.*.m
