# pika core — library, CLI, and optional TUI.
# SPDX-License-Identifier: GPL-3.0-or-later

CC      ?= cc
AR      ?= ar
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
CFLAGS  += -fPIC
CPPFLAGS+= -Iinclude -D_GNU_SOURCE
LDFLAGS ?=

PREFIX     ?= /usr/local
DESTDIR    ?=
BINDIR      = $(DESTDIR)$(PREFIX)/bin
LIBDIR      = $(DESTDIR)$(PREFIX)/lib
INCLUDEDIR  = $(DESTDIR)$(PREFIX)/include
PKGDIR      = $(LIBDIR)/pkgconfig

VERSION = 1.0.0

LIB_SRC = $(wildcard src/*.c)
LIB_OBJ = $(LIB_SRC:.c=.o)

STATIC  = libpika.a
SHARED  = libpika.so
CLI     = pika
TUI     = pika-tui
PC      = pika.pc

.PHONY: all lib cli tui clean check install install-tui uninstall $(PC)

all: lib cli tui          ## build everything

lib: $(STATIC) $(SHARED)  ## build the static and shared library
cli: $(CLI)               ## build the pika CLI
tui: $(TUI)               ## build the pika-tui reader

$(STATIC): $(LIB_OBJ)
	$(AR) rcs $@ $^

$(SHARED): $(LIB_OBJ)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

$(CLI): cli/main.o $(STATIC)
	$(CC) $(CFLAGS) -o $@ cli/main.o $(STATIC) $(LDFLAGS)

$(TUI): tui/main.o $(STATIC)
	$(CC) $(CFLAGS) -o $@ tui/main.o $(STATIC) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# ---- tests ----------------------------------------------------------------
check: $(STATIC)          ## build and run unit tests
	$(CC) $(CFLAGS) $(CPPFLAGS) -o tests/run tests/test_core.c $(STATIC) $(LDFLAGS)
	./tests/run

# ---- pkg-config -----------------------------------------------------------
$(PC):
	@printf 'prefix=%s\n' "$(PREFIX)"            >  $@
	@printf 'exec_prefix=$${prefix}\n'           >> $@
	@printf 'libdir=$${prefix}/lib\n'            >> $@
	@printf 'includedir=$${prefix}/include\n\n'  >> $@
	@printf 'Name: pika\n'                       >> $@
	@printf 'Description: Focal-point (RSVP) reading engine\n' >> $@
	@printf 'Version: %s\n' "$(VERSION)"         >> $@
	@printf 'Libs: -L$${libdir} -Wl,-rpath,$${libdir} -lpika\n' >> $@
	@printf 'Libs.private: -L$${libdir} -lpika\n' >> $@
	@printf 'Cflags: -I$${includedir}\n'         >> $@

# ---- install --------------------------------------------------------------
install: lib cli $(PC)    ## install library, headers, CLI, pkg-config
	install -d $(LIBDIR) $(INCLUDEDIR)/pika $(BINDIR) $(PKGDIR)
	install -m 644 $(STATIC) $(LIBDIR)/
	install -m 755 $(SHARED) $(LIBDIR)/
	install -m 644 include/pika/*.h $(INCLUDEDIR)/pika/
	install -m 755 $(CLI) $(BINDIR)/
	install -m 644 $(PC) $(PKGDIR)/

install-tui: tui          ## install the optional TUI reader
	install -d $(BINDIR)
	install -m 755 $(TUI) $(BINDIR)/

uninstall:
	rm -f $(LIBDIR)/$(STATIC) $(LIBDIR)/$(SHARED) $(BINDIR)/$(CLI) \
	      $(BINDIR)/$(TUI) $(PKGDIR)/$(PC)
	rm -rf $(INCLUDEDIR)/pika

clean:
	rm -f $(LIB_OBJ) cli/main.o tui/main.o $(STATIC) $(SHARED) \
	      $(CLI) $(TUI) $(PC) tests/run
