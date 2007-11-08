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

.SUFFIXES: .xmli .idlxml

default: package
COYOTOS_SRC=../../..
CLEANLIST += *.xmli
CLEANLIST += *.idlxml

OSDOC_XML_SRC=interfaces
OSDOC_SBOX_SRC=$(wildcard *.sbox)
OSDOC_FIG_SRC=$(wildcard *.fig)

include $(COYOTOS_SRC)/build/make/makerules.mk

all:

install: all

XSL_IDL_OSDOC=../ukernel/idl-osdoc.xsl
XSL_IDL_OPTS= \
	-param seclevel 0  \
	-param manpages 1

SYSIDL=$(COYOTOS_SRC)/sys/idl
BASEIDL=$(COYOTOS_SRC)/base/idl
IDL=$(notdir $(wildcard $(BASEIDL)/[A-Za-z]*.idl))

interfaces.ltx: $(IDL:.idl=.xmli) $(XSL_IDL_OSDOC)
interfaces.html: $(IDL:.idl=.xmli) $(XSL_IDL_OSDOC)

#%.xmli: $(BASEIDL)/%.idl $(XSL_IDL_OSDOC)
#	$(RUN_CAPIDL) -I $(BASEIDL) -l xml $< | $(XSLTPROC) $(LATEX_XSLTPROC_OPTS) $(XSL_IDL_OPTS) --output $@ $(XSL_IDL_OSDOC) -

%.xmli: %.idlxml $(XSL_IDL_OSDOC)
	$(XSLTPROC) $(OSDOC_LATEX_OPTS) $(XSL_IDL_OPTS) --output $@ $(XSL_IDL_OSDOC) $<

# This is moderately tricky. The dependency here is between the 
# files (at least for the moment), but the new capidl command line
# wants to be passed the interface name.
%.idlxml: $(BASEIDL)/%.idl
	$(RUN_CAPIDL) -I $(BASEIDL) -I $(SYSIDL) -l xml $< > $@

cleanif:
	-rm -f *.idlxml *.xmli

.SECONDARY: $(IDL:.idl=.xmli) $(IDL:.idl=.idlxml)
