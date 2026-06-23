# Top-level convenience wrapper. The library lives in core/; everything else
# (web server, frontend) is optional and has its own tooling.
# SPDX-License-Identifier: GPL-3.0-or-later

.PHONY: all lib cli tui check install install-tui uninstall clean help

all:         ## build the library, CLI, and TUI
	$(MAKE) -C core all

lib:         ## build just the library
	$(MAKE) -C core lib

cli:         ## build just the CLI
	$(MAKE) -C core cli

tui:         ## build just the TUI reader
	$(MAKE) -C core tui

check:       ## run the unit tests
	$(MAKE) -C core check

install:     ## install library + headers + CLI (PREFIX=/usr/local)
	$(MAKE) -C core install

install-tui: ## install the optional TUI reader
	$(MAKE) -C core install-tui

uninstall:
	$(MAKE) -C core uninstall

clean:
	$(MAKE) -C core clean

help:        ## list targets
	@grep -hE '^[a-z-]+:.*##' $(MAKEFILE_LIST) | \
		sed 's/:.*##/\t/' | sort
