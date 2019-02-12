include Makefile.am.coverage
ACLOCAL_AMFLAGS=-I m4
SUBDIRS = lib src tst inc www man

if HAVE_GCOV
clean-local: clean-gcov
endif

#sysconf_DATA = init.d/kurload.conf
#init_ddir = $(sysconfdir)/init.d
#dist_init_d_SCRIPTS = init.d/kurload
#man_MANS = kurload.1
#EXTRA_DIST = man2html.sh gen-download-page.sh readme.md 
EXTRA_DIST = tap-driver.sh

analyze:
	make analyze -C src
	make analyze -C lib
	make analyze -C tst

www:
#	./gen-download-page.sh
	./man2html.sh
	make www -C www

.PHONY: analyze www
