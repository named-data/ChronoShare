#!/usr/bin/make -f
# Waf Makefile wrapper

all:
	@./waf build

all-debug:
	@./waf -v build

all-progress:
	@./waf -p build

install:
	./waf install --yes;

uninstall:
	./waf uninstall

clean:
	@./waf clean

distclean:
	@./waf distclean
	@-rm -rf build

check:
	@./waf check

dist:
	@./waf dist

.PHONY: clean dist distclean check uninstall install all

