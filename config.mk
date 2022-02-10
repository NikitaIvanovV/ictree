PREFIX    ?= /usr/local
BINPREFIX := $(DESTDIR)$(PREFIX)/bin
MANPREFIX := $(DESTDIR)$(PREFIX)/share/man

BIN := ictree

# Some directories
SRCDIR   := src
INCDIR   := include
DOCDIR   := doc
GENDIR   := gen
BUILDDIR := build

# Flags
CFLAGS += -std=gnu99 -pedantic -Wall -Wextra -Wno-unused-parameter
CFLAGS += -I${INCDIR} -I.

ifeq ($(ENV),dev)
CFLAGS += -Og -g -DDEV
else
CFLAGS  += -Os
LDFLAGS += -s
endif

# Termbox2 lib
TBDIR  := termbox2
TBOBJ  := ${TBDIR}/termbox.o
CFLAGS += -I${TBDIR}

# c_vector lib
CVDIR  := c-vector
CFLAGS += -I${CVDIR}
