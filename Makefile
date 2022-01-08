include config.mk

SRC  := $(wildcard ${SRCDIR}/*.c)
OBJ  := ${SRC:${SRCDIR}/%.c=${BUILDDIR}/%.o}
DEP  := ${OBJ:.o=.d}
MAN  := $(wildcard ${DOCDIR}/*)
LOBJ := ${TBOBJ}

vpath %.c ${SRCDIR}

all: ${BIN}

options:
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"

install: install.bin install.man

install.bin: ${BIN}
	install -d $(BINPREFIX)
	install $< $(BINPREFIX)

install.man: $(wildcard ${DOCDIR}/*)
	install -d $(MANPREFIX)
	# install $^ $(MANPREFIX)

uninstall:
	$(RM) $(BINPREFIX)/${BIN}

clean:
	$(RM) ${BIN} ${OBJ} ${DEP}
	$(MAKE) -C ${TBDIR} clean

.PHONY: all options install install.bin install.man uninstall clean

${BIN}: ${OBJ} ${LOBJ}
	$(CC) -o $@ $(LDFLAGS) $+

${BUILDDIR}/%.o: %.c
	@mkdir -p ${@D}
	$(CC) -c -o $@ $(CFLAGS) -MD $<

-include ${DEP}

${TBOBJ}:
	$(MAKE) -C ${TBDIR} termbox.o
