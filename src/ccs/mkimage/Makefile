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

include $(COYOTOS_SRC)/build/make/makevars.mk

# This needs to change when we fix the install targets
CXXFLAGS+=-g
#CXXFLAGS+=-Wuninitialized -O
#CXXFLAGS+=-finline-limit=8000 --param inline-unit-growth=400 --param max-inline-insns-single=16000 --param large-function-growth=4000

INC=-I. -I.. -I$(COYOTOS_SRC)/sys $(INC_SHERPA)
DEF+=-DCROSS_COMPILING -DCOYOTOS_UNIVERSAL_CROSS

include $(COYOTOS_SRC)/build/make/makerules.mk

CLEANLIST=y.tab.c y.tab.h y.output
#CLEANLIST+=BitcLexer.c
#CLEANLIST=./AST.cxx ./AST.hxx

LIBCOYIMAGE=../libcoyimage/BUILD/libcoyimage.a

# Uncomment the following for debugging:
#LIBSHERPA=../../ccs-xenv/coytools/src/libsherpa/libsherpa.a

LIBS+= $(LIBCOYIMAGE)
LIBS+= $(LIBSHERPA)
LIBS+= -licuuc -lstdc++ -lz

OBJECTS=\
	$(BUILDDIR)/mkimage.o \
	$(BUILDDIR)/Lexer.o \
	$(BUILDDIR)/Parser.o \
	$(BUILDDIR)/builtin.o \
	$(BUILDDIR)/builtin-endpoint.o \
	$(BUILDDIR)/builtin-gpt.o \
	$(BUILDDIR)/builtin-process.o \
	$(BUILDDIR)/builtin-diag.o \
	$(BUILDDIR)/builtin-capvalue.o \
	$(BUILDDIR)/UocInfo.o \
	$(BUILDDIR)/Value.o \
	$(BUILDDIR)/Interp.o \
	$(BUILDDIR)/ConstDef.o \
	$(BUILDDIR)/FQName.o \
	$(BUILDDIR)/AST.o \
	$(BUILDDIR)/Environment.o \
	$(BUILDDIR)/PrettyPrint.o \

TARGETS=$(BUILDDIR)/mkimage

install all: $(TARGETS)

install: all
	$(INSTALL) -d $(COYOTOS_ROOT)/host
	$(INSTALL) -d $(COYOTOS_ROOT)/host/bin
	$(INSTALL) -m 0755 $(TARGETS) $(COYOTOS_ROOT)/host/bin

$(BUILDDIR)/mkimage: AST.hxx $(OBJECTS) $(LIBCOYIMAGE)
	$(GPLUS) $(GPLUSFLAGS) $(OBJECTS) $(LIBS) -o $@

#This is a placeholder.

$(BUILDDIR)/%.o: %.cxx
	$(CXX_DEP)
	$(CXX_BUILD)

$(BUILDDIR)/Parser.o: $(BUILDDIR)/Parser.cxx
	$(CXX_DEP)
	$(CXX_BUILD) 

$(BUILDDIR)/Parser.cxx: $(MAKE_BUILDDIR)
$(BUILDDIR)/Parser.cxx $(BUILDDIR)/Parser.hxx: Parser.y $(MAKE_BUILDDIR) 
	(cd $(BUILDDIR); bison -t -p mki -v -d -o Parser.cxx -y $(RBUILDDIR)/Parser.y)

AST.cxx: AST.ast AST.hxx
	$(ASTMAKER) -s AST.ast

AST.hxx: AST.ast
	$(ASTMAKER) -h AST.ast

$(LIBCOYIMAGE):
	$(MAKE) -C ../libcoyimage


# Following is a dummy dependency. It ensures that the header file
# containing token numbers gets generated before we try to compile
# BitcLexer.cxx
Lexer.cxx: $(BUILDDIR)/Parser.cxx

-include $(BUILDDIR)/.*.m

