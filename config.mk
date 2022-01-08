#
# Copyright 2022 Nikita Ivanov
#
# This file is part of ictree
#
# ictree is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# ictree is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# ictree. If not, see <https://www.gnu.org/licenses/>.
#

PREFIX    ?= /usr/local
BINPREFIX := $(DESTDIR)$(PREFIX)/bin
MANPREFIX := $(DESTDIR)$(PREFIX)/share/man

BIN := ictree

# Some directories
SRCDIR   := src
INCDIR   := include
DOCDIR   := doc
BUILDDIR := build

# Termbox2 lib
TBDIR  := termbox2
TBOBJ  := ${TBDIR}/termbox.o
CFLAGS += -I${TBDIR}

# c_vector lib
CVDIR  := c-vector
CFLAGS += -I${CVDIR}

# Flags
CFLAGS += -std=c99 -pedantic -Wall -Wextra -Wno-unused-parameter
CFLAGS += -I${INCDIR}

ifeq ($(ENV),dev)
CFLAGS += -Og -g -DDEV
else
CFLAGS  += -Os
LDFLAGS += -s
endif
