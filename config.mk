PREFIX    ?= /usr/local
BINPREFIX := $(DESTDIR)$(PREFIX)/bin
MANPREFIX := $(DESTDIR)$(PREFIX)/share/man

BIN := ictree

# Some directories
SRCDIR   := src
INCDIR   := include
DOCDIR   := doc
GENDIR   := gen
LIBDIR   := lib
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
TBDIR  := ${LIBDIR}/termbox2
TBARC  := ${TBDIR}/libtermbox.a
CFLAGS += -I${TBDIR}

# c_vector lib
CVDIR  := ${LIBDIR}/c-vector
CFLAGS += -I${CVDIR}
