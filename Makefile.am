include Makefile.am.coverage
ACLOCAL_AMFLAGS=-I m4
SUBDIRS = lib src tst inc www man

if HAVE_GCOV
clean-local: clean-gcov
endif

EXTRA_DIST = tap-driver.sh psmq-common.h readme.md man2html.sh

analyze:
	make analyze -C src
	make analyze -C lib
	make analyze -C tst

www:
#	./gen-download-page.sh
	./man2html.sh
	make www -C www

.PHONY: analyze www
