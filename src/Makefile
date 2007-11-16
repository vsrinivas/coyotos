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


default: world

COYOTOS_SRC=.

# DIRS are ordered:
# build must be made before others. base and doc can be in either order.
# Nothing depends on legal.
DIRS=ccs sys base tutorial
VAR_ADDED_DIRS=$(sort $(wildcard var.*))
DIRS+=$(VAR_ADDED_DIRS)

ifeq "$(COYOTOS_ENABLE_DOCS)" "yes"
then
DIRS+=web
endif

CLEANDIRS=build $(DIRS)
TARDIRS=$(DIRS)

include $(COYOTOS_SRC)/build/make/pkgrules.mk

# This needs to follow the include line so that COYOTOS_ROOT is defined:
HGTREES=coyotos
OPT_HGTREES=coyotos/src/ccs-xenv coyotos/src/tutorial
VAR_HGTREES=$(patsubst %,coyotos/src/%,$(VAR_ADDED_DIRS))
ALL_HGTREES=$(HGTREES) $(OPT_HGTREES) $(VAR_HGTREES)
HG_TREETOP=$(COYOTOS_ROOT)/..

targdir-clean:
	-rm -rf $(COYOTOS_ROOT)/include $(COYOTOS_ROOT)/lib
	-rm -rf $(COYOTOS_ROOT)/doc $(COYOTOS_ROOT)/doc
	-rm -rf $(COYOTOS_ROOT)/host $(COYOTOS_ROOT)/usr
	-rm -rf $(COYOTOS_ROOT)/idl

#ifeq "$(NEED_CROSS_BOOTSTRAP)" "yes"
#cross-bootstrap:
#	$(MAKE) -C base/ccs PACKAGE=base $(MAKERULES) cross-bootstrap
#endif
#
packages: | ccs-pkg
	$(MAKE) $(MAKERULES) interfaces
	$(MAKE) $(MAKERULES) libs
	$(MAKE) $(MAKERULES) install

interfaces: | ccs-pkg

# This no longer requires any special building
#build-pkg:
#	$(MAKE) -C build PACKAGE=build $(MAKERULES) package

ccs-pkg:
	$(MAKE) -C ccs PACKAGE=ccs $(MAKERULES) package

# The install target recurses deeply in any case.
install:

tar:	clean
	-rm -f eros.tar eros.tar.gz eros.tgz
	tar cvf eros.tar Makefile $(TARDIRS)
	gzip eros.tar
	mv eros.tar.gz eros.tgz

# Targets specific to OpenCM:
HOSTNAME=$(shell hostname)
ifeq "$(HOSTNAME)" "office.eros-os.com"
CMR=cm
else
CMR=cmr
endif

status:
	@for tree in `echo $(ALL_HGTREES)`; do\
	    if [ -d $(HG_TREETOP)/$${tree}/.hg ]; then \
	        echo "[$${tree}]"; \
		hg --cwd $(HG_TREETOP)/$${tree} status; \
	    fi; \
	done

update:
	@for tree in `echo $(ALL_HGTREES)`; do\
	    if [ -d $(HG_TREETOP)/$${tree}/.hg ]; then \
	        echo "[$${tree}]"; \
		hg --cwd $(HG_TREETOP)/$${tree} fetch; \
	    fi; \
	done

get_trees:
	build/bin/get-trees.py
