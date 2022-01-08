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
