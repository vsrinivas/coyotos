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

CROSS_PREFIX=$(COYOTOS_GCC_ARCH)-unknown-coyotos-

COYOTOS_GCC=$(COYOTOS_XENV)/host/bin/$(CROSS_PREFIX)gcc
COYOTOS_GPLUS=$(COYOTOS_XENV)/host/bin/$(CROSS_PREFIX)g++
COYOTOS_LD=$(COYOTOS_XENV)/host/bin/$(CROSS_PREFIX)ld
COYOTOS_AR=$(COYOTOS_XENV)/host/bin/$(CROSS_PREFIX)ar
COYOTOS_SIZE=$(COYOTOS_XENV)/host/bin/$(CROSS_PREFIX)size
COYOTOS_OBJDUMP=$(COYOTOS_XENV)/host/bin/$(CROSS_PREFIX)objdump
COYOTOS_RANLIB=$(COYOTOS_XENV)/host/bin/$(CROSS_PREFIX)ranlib

COYOTOS_CPP=/lib/cpp -undef -nostdinc -D$(COYOTOS_ARCH)
COYOTOS_GCC_OPTIM=-finline-limit=3000 -fno-strict-aliasing $(COYOTOS_GCC_MODEL)
COYOTOS_GCC_KERNEL_ALIGN=-falign-functions=4
COYOTOS_ASM_OPTIONS=$(COYOTOS_GCC_MODEL)

GAWK=gawk

HOST_FD=/dev/fd0H1440
