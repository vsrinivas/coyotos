#
# Copyright (C) 2007, The EROS Group, LLC
# All rights reserved.
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

# Cancel the old suffix list, because the order matters.  We want assembly 
# source to be recognized in preference to ordinary source code, so the
# .S, .s cases must come ahead of the rest.
.SUFFIXES:
.SUFFIXES: .S .s .cxx .c .y .l .o .dvi .ltx .gif .fig .xml .html .pdf .xhtml

# Some shells do not export this variable. It is used in the package
# generation rule and in the discovery of COYOTOS_ROOT.
PWD=$(shell pwd)

#
# Set up default values for these variables so that a build in an improperly
# configured environment has a fighting chance:
#
ifdef COYOTOS_ROOT

COYOTOS_SRCDIR=$(word 1, $(strip $(subst /, ,$(subst $(COYOTOS_ROOT)/,,$(PWD)))))
COYOTOS_FINDDIR=old

else

ifneq "" "$(findstring /coyotos/pkgsrc,$(PWD))"
COYOTOS_SRCDIR=pkgsrc
else
COYOTOS_SRCDIR=src
endif

COYOTOS_ROOT=$(firstword $(subst /coyotos/$(COYOTOS_SRCDIR), ,$(PWD)))/coyotos

endif

#ifeq "$(word 2 $(subst -, ,$(COYOTOS_TARGET)))",""
#$(error COYOTOS_TARGET should now take the form ARCHITECTURE-BSP)
#endif

ifndef COYOTOS_TARGET
COYOTOS_TARGET=i386
endif

COYOTOS_ARCH=$(word 1,$(subst -, ,$(COYOTOS_TARGET)))
ifeq "$(COYOTOS_ARCH)" "coldfire"
COYOTOS_GCC_ARCH=m68k
else
COYOTOS_GCC_ARCH=$(COYOTOS_ARCH)
endif

ifeq "$(word 2,$(subst -, ,$(COYOTOS_TARGET)))" ""
COYOTOS_BSP=default
else
COYOTOS_BSP=$(word 2,$(subst -, ,$(COYOTOS_TARGET)))
endif

ifndef COYOTOS_ROOT
endif
ifndef COYOTOS_XENV
ifneq "" "$(findstring /coyotos/host/bin,$(wildcard /coyotos/host/bin*))"
COYOTOS_XENV=/coyotos
endif
endif
#ifndef COYOTOS_CONFIG
#COYOTOS_CONFIG=DEFAULT
#endif

VMWARE=$(COYOTOS_ROOT)/src/build/bin/vmdbg
export COYOTOS_ROOT
export COYOTOS_TARGET
export COYOTOS_ARCH
export COYOTOS_BSP
export COYOTOS_XENV
export COYOTOS_CONFIG

DIRSYNC=$(COYOTOS_SRC)/build/bin/dirsync
INSTALL=$(COYOTOS_SRC)/build/bin/coyinstall
REPLACE=$(COYOTOS_SRC)/build/bin/move-if-change
MKIMAGEDEP=$(COYOTOS_SRC)/build/bin/mkimagedep

#
# First, set up variables for building native tools:
#
GAWK=gawk
PYTHON=python
XML_LINT=xmllint
XSLTPROC=xsltproc --xinclude

NATIVE_STRIP=strip
NATIVE_OBJDUMP=objdump
NATIVE_SIZE=size
NATIVE_AR=ar
NATIVE_LD=ld
NATIVE_REAL_LD=ld
NATIVE_RANLIB=ranlib

NATIVE_GCC=gcc --std=gnu99
ifndef GCCWARN
MOPSWARN=-Wall -Winline -Wno-format -Wno-char-subscripts
GCCWARN=$(MOPSWARN)
endif

NATIVE_GPLUS=g++
ifndef GPLUSWARN
GPLUSWARN=-Wall -Winline -Wno-format
endif

COYOTOS_WARN_ERROR=-Werror
#
# Then variables for building Coyotos binaries:
#
COYOTOS_GCC=$(NATIVE_GCC)
COYOTOS_GPLUS=$(NATIVE_GPLUS)
COYOTOS_LD=$(NATIVE_LD)
COYOTOS_REAL_LD=$(NATIVE_LD)
COYOTOS_AR=$(NATIVE_AR)
COYOTOS_RANLIB=$(NATIVE_RANLIB)
COYOTOS_OBJDUMP=$(NATIVE_OBJDUMP)
COYOTOS_STRIP=$(NATIVE_STRIP)
COYOTOS_SIZE=$(NATIVE_SIZE)

#
# Then variables related to installation and test generation:
#

HOST_FD=/dev/fd0H1440 

###############################################################
#
# DANGER, WILL ROBINSON!!!!
#
# The COYOTOS_HD environment variable is defined to something
# harmless here **intentionally**.  There are too many ways
# to do grievous injuries to a misconfigured host environment
# by setting a default.
#
# It is intended that the intrepid UNIX-based developer should
# pick a hard disk partition, set that up with grub or some
# such, make it mode 666 from their UNIX environment, and
# then set COYOTOS_HD to name that partition device file.  This
# is how *I* work, but you do this at your own risk!!
#
###############################################################
ifndef COYOTOS_HD
COYOTOS_HD=/dev/null
endif

CAPIDL=$(COYOTOS_ROOT)/host/bin/capidl
RUN_CAPIDL=$(CAPIDL) -a $(COYOTOS_ARCH)
MKIMAGE=$(COYOTOS_ROOT)/host/bin/mkimage
RUN_MKIMAGE=$(MKIMAGE) -t $(COYOTOS_ARCH)

#
# This is where the target environment makefile gets a chance to override
# things:
#
ifndef COYOTOS_HOSTENV
COYOTOS_HOSTENV=env-cross
endif

include $(COYOTOS_SRC)/build/make/$(COYOTOS_HOSTENV).mk

# search for ppmtogif in all the obvious places:
ifndef NETPBMDIR
ifneq "" "$(findstring /usr/bin/ppmtogif,$(wildcard /usr/bin/*))"
NETPBMDIR=/usr/bin
endif
endif

ifndef NETPBMDIR
ifneq "" "$(findstring /usr/bin/X11/ppmtogif,$(wildcard /usr/bin/X11/*))"
NETPBMDIR=/usr/bin/X11
endif
endif

ifndef NETPBMDIR
ifneq "" "$(findstring /usr/local/netpbm/ppmtogif,$(wildcard /usr/local/netpbm/*))"
NETPBMDIR=/usr/local/netpbm
endif
endif

ifndef COYOTOS_FD
COYOTOS_FD=$(HOST_FD)
endif

# Record the location of our DEPGEN script:
COYOTOS_RUNLATEX=$(COYOTOS_SRC)/build/make/runlatex.sh
COYOTOS_PS2PDF=$(COYOTOS_SRC)/build/make/ps2pdf.sh
COYOTOS_XCACHE=$(COYOTOS_SRC)/build/make/xcache.sh

# search for ccache in the obvious places:
ifndef COYOTOS_CCACHE
ifneq "" "$(findstring /usr/bin/ccache,$(wildcard /usr/bin/ccache*))"
COYOTOS_CCACHE=/usr/bin/ccache
endif
endif

ifndef COYOTOS_CCACHE
ifneq "" "$(findstring /usr/local/bin/ccache,$(wildcard /usr/local/bin/ccache*))"
COYOTOS_CCACHE=/usr/local/bin/ccache
endif
endif

#
# Now for the REALLY SLEAZY part: if this makefile is performing a
# cross build, smash the native tool variables with the cross tool 
# variables.  The clean thing to do would be to separate the rules 
# somehow, but this is quicker:
ifdef CROSS_BUILD
GCC=$(COYOTOS_CCACHE) $(COYOTOS_GCC)
GPLUS=$(COYOTOS_CCACHE) $(COYOTOS_GPLUS)
LD=$(COYOTOS_LD)
AR=$(COYOTOS_AR)
OBJDUMP=$(COYOTOS_OBJDUMP)
SIZE=$(COYOTOS_SIZE)
STRIP=$(COYOTOS_STRIP)
RANLIB=$(COYOTOS_RANLIB)
GPLUS_OPTIM=$(COYOTOS_GPLUS_OPTIM)
GCC_OPTIM=$(COYOTOS_GCC_OPTIM)
STDLIBDIRS=-L $(COYOTOS_ROOT)/usr/lib
endif
ifndef CROSS_BUILD
GCC=$(COYOTOS_CCACHE) $(NATIVE_GCC)
GPLUS=$(COYOTOS_CCACHE) $(NATIVE_GPLUS)
LD=$(NATIVE_LD)
AR=$(NATIVE_AR)
OBJDUMP=$(NATIVE_OBJDUMP)
SIZE=$(NATIVE_SIZE)
STRIP=$(NATIVE_STRIP)
RANLIB=$(NATIVE_RANLIB)
GPLUS_OPTIM=$(NATIVE_GPLUS_OPTIM)
GCC_OPTIM=$(NATIVE_GCC_OPTIM)
STDLIBDIRS=
endif

ifneq "" "$(findstring $(COYOTOS_XENV)/host/bin/astmaker,$(wildcard $(COYOTOS_XENV)/host/bin/*))"
ASTMAKER=$(COYOTOS_XENV)/host/bin/astmaker
else
ASTMAKER=astmaker
endif

ifneq "" "$(findstring $(COYOTOS_XENV)/host/lib/libsherpa.a,$(wildcard $(COYOTOS_XENV)/host/lib/*))"
LIBSHERPA=$(COYOTOS_XENV)/host/lib/libsherpa.a
else
LIBSHERPA=-lsherpa
endif

ifneq "" "$(findstring $(COYOTOS_XENV)/host/include/libsherpa,$(wildcard $(COYOTOS_XENV)/host/include/*))"
INC_SHERPA=-I$(COYOTOS_XENV)/host/include
else
INC_SHERPA=
endif

## This is a holdover from the EROS tree that we may not want:
## DOMLIB= $(COYOTOS_ROOT)/lib/libdomain.a
## DOMLIB += $(COYOTOS_ROOT)/lib/libidlstub.a
## DOMLIB += $(COYOTOS_ROOT)/lib/libdomgcc.a
## DOMLIB += $(COYOTOS_ROOT)/lib/libc.a

#FIX: Need to define DOMCRT0 and DOMCRTN and DOMLINKOPT
# ifeq "$(COYOTOS_HOSTENV)" "linux-xenv-gcc3"
# #DOMCRT0=$(COYOTOS_ROOT)/lib/gcc-lib/$(COYOTOS_ARCH)-unknown-eros/3.3/crt1.o
# #DOMCRTN=$(COYOTOS_ROOT)/lib/gcc-lib/$(COYOTOS_ARCH)-unknown-eros/3.3/crtn.o
# DOMLINKOPT=-N -static -Ttext 0x0 -L$(COYOTOS_ROOT)/usr/lib
# DOMLINK=$(GCC)
# else
# DOMCRT0=$(COYOTOS_ROOT)/lib/crt0.o
# DOMCRTN=$(COYOTOS_ROOT)/lib/crtn.o
# DOMLINKOPT=-N -Ttext 0x0 -nostdlib -static -e _start -L$(COYOTOS_ROOT)/usr/lib -L$(COYOTOS_ROOT)/usr/lib/$(COYOTOS_ARCH)
# DOMLINK=$(COYOTOS_LD)
# endif

DOMLIB += $(DOMCRTN)


# Really ugly GNU Makeism to extract the name of the current package by
# stripping $COYOTOS_ROOT/ out of $PWD (yields: src/PKG/xxxxxx), turning /
# characters into spaces (yields: "src PKG xxx xxx xxx"), and  
# then selecting the appropriate word. Notice that the first substring is 
# empty, so the appropriate word is the second one.
PACKAGE=$(word 1, $(strip $(subst /, ,$(subst $(COYOTOS_ROOT)/src/,,$(PWD)))))

PKG_ROOT=$(COYOTOS_ROOT)/pkg/${PACKAGE}
PKG_SRC=$(COYOTOS_ROOT)/$(COYOTOS_SRCDIR)/${PACKAGE}

# Until proven otherwise...
IMGMAP=imgmap

# Until proven otherwise...
BOOTSTRAP=boot

# Until proven otherwise...

ifeq "$(BUILDDIR)" ""
ifeq "$(PACKAGE)" "web"
BUILDDIR=.
RBUILDDIR=.
endif
ifeq "$(PACKAGE)" "legal"
BUILDDIR=.
RBUILDDIR=.
endif
ifeq "$(PACKAGE)" "build"
BUILDDIR=.
RBUILDDIR=.
endif
ifeq "$(BUILDDIR)" ""
BUILDDIR=BUILD
RBUILDDIR=..
endif
endif

showme:
	@echo "COYOTOS_ROOT: " $(COYOTOS_ROOT)
	@echo "COYOTOS_SRCDIR: " $(COYOTOS_SRCDIR)
	@echo "PKG_ROOT:" $(PKG_ROOT)
	@echo "PKG_SRC:" $(PKG_SRC)
	@echo "BUILDDIR:" $(BUILDDIR)

MAKEVARS_LOADED=1
