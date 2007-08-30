#
# Copyright (C) 2005, Jonathan S. Shapiro.
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

# Include this file into your makefile to pick up the document building rules.
# You need to define the variable DOCTOOLS first.

ifneq "" "$(findstring $(COYOTOS_XENV)/host/share/osdoc,$(wildcard $(COYOTOS_XENV)/host/share/*))"
OSDOCTOOLS=$(COYOTOS_XENV)/host/share/osdoc
else
OSDOCTOOLS=/usr/share/osdoc
endif

# If we are in the doc/web tree, we want a relative path 
# to the web root, otherwise we want an absolute path:#
WEBROOT=$(COYOTOS_SRC)/doc/web/
ifeq "$(PACKAGE)" "doc"
ifeq "$(COYOTOS_SRC)" "../.."
WEBROOT=.
else
WEBROOT=$(subst /../../doc/web/,,$(COYOTOS_SRC)/doc/web/)
endif
endif

ifeq "$(HTML_TOPNAV)" ""
HTML_TOPNAV=docs.html
endif

ifeq "$(OSDOC_NAVCONTEXT)" ""
OSDOC_NAVCONTEXT="none"
endif

# This is a legacy support thing. The "else" case should be removed.
ifneq "$(OSDOC_NAVCONTEXT)" ""
OSDOC_NAVCONTEXT_PARAM=--stringparam navcontext $(OSDOC_NAVCONTEXT)
OSDOC_TOPNAV_PARAM=--stringparam topnav $(WEBROOT)/topnav.html \
                   --stringparam topnavdir $(WEBROOT)
else
OSDOC_TOPNAV_PARAM=--stringparam topnav $(WEBROOT)/topnav/$(HTML_TOPNAV) \
                   --stringparam topnavdir $(WEBROOT)/topnav
endif

ifneq "$(OSDOC_SIDENAV)" ""
OSDOC_SIDENAV_PARAM=--stringparam sidenav $(OSDOC_SIDENAV) \
                    --stringparam sidenavdir .
endif

OSDOC_SITEHDFT_PARAM=--stringparam siteheader $(WEBROOT)/siteheader.html \
                     --stringparam sitefooter $(WEBROOT)/sitefooter.html

LATEX_XSLTPROC_OPTS+=
ifeq "$(PACKAGE)" "doc"
OSDOC_HTML_OPTS+=\
	--param navbars 1 \
	$(OSDOC_SITEHDFT_PARAM) \
	$(OSDOC_NAVCONTEXT_PARAM) \
	$(OSDOC_TOPNAV_PARAM) \
	$(OSDOC_SIDENAV_PARAM) \
	--stringparam webroot $(WEBROOT)
OSDOC_XHTML_OPTS+=\
	--param navbars 1 \
	$(OSDOC_SITEHDFT_PARAM) \
	$(OSDOC_NAVCONTEXT_PARAM) \
	$(OSDOC_TOPNAV_PARAM) \
	$(OSDOC_SIDENAV_PARAM) \
	--stringparam webroot $(WEBROOT)
endif
OSDOC_HTML_CHUNK_OPTS+= --param enable.chunking 1

ifndef STYLESHEET
# Definite path to default style sheet would be
#    $(COYOTOS_SRC)/doc/web/styles.css
# We assume here that if a style sheet is going to be used, it will
# be used from within the doc subtree, in which case we need the 
# relative path from the build directory to
#    $(COYOTOS_SRC)/doc/web.
# We construct this using substitution:

STYLESHEET=$(WEBROOT)/styles.css
endif

webroot:
	@echo "webroot: $(WEBROOT)"
	@echo "pwd:     $(PWD)"
	@echo "web:     $(COYOTOS_SRC)/doc/web"
	@echo "pkg:     $(PACKAGE)"
	@echo "STYLESHEET=$(WEBROOT)/styles.css"

include $(OSDOCTOOLS)/osdoc.mk

OSDOC_RULES_LOADED=1
