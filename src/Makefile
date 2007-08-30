#
# Copyright (C) 2007, The EROS Group, LLC.
#
# This file is part of the EROS Operating System.
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
REQUIRED_DIRS=ccs sys base
OPTIONAL_DIRS=doc tutorial
DIRS=$(REQUIRED_DIRS) $(OPTIONAL_DIRS)
CLEANDIRS=build $(DIRS)

UPDATE_DIRS=build $(REQUIRED_DIRS)
OPT_UPDATE_DIRS=ccs-xenv $(OPTIONAL_DIRS)
ALL_UPDATE_DIRS=$(UPDATE_DIRS) $(OPT_UPDATE_DIRS)

TARDIRS=$(DIRS)

include $(COYOTOS_SRC)/build/make/pkgrules.mk

distclean: targdir-clean clean

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
	$(MAKE) interfaces
	$(MAKE) libs
	$(MAKE) install

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
	@for d in `echo $(ALL_UPDATE_DIRS)`; do\
	    if [ -d $$d ]; then \
	        echo "Status of $$d:"; \
		cm -C $$d status; \
	    fi; \
	done

update:
	@for d in `echo $(ALL_UPDATE_DIRS)`; do\
	    if [ -d $$d ]; then \
	        echo "Updating $$d"; \
		(cd $$d; $(CMR) update); \
	    fi; \
	done

destructive-sync:
	rsync -avz rsync://www.coyotos.org/src.coyotos/src/Makefile ./Makefile
	for d in `echo $(UPDATE_DIRS) build`; do\
	   rsync -avz --delete rsync://www.coyotos.org/src.coyotos/src/$$d/ $$d/; \
	done
	for d in `echo $(OPT_UPDATE_DIRS)`; do\
	    if [ -d $$d ]; then \
	        rsync -avz --delete rsync://www.coyotos.org/src.coyotos/src/$$d/ $$d/; \
	    fi; \
	done

sync:
	rsync -avz rsync://www.coyotos.org/src.coyotos/src/Makefile ./Makefile
	for d in `echo $(UPDATE_DIRS) build`; do\
	    rsync -avz rsync://www.coyotos.org/src.coyotos/src/$$d/ $$d/; \
	done
	for d in `echo $(OPT_UPDATE_DIRS)`; do\
	    if [ -d $$d ]; then \
	        rsync -avz rsync://www.coyotos.org/src.coyotos/src/$$d/ $$d/; \
	    fi; \
	done
