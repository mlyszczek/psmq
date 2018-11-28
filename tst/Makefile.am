check_PROGRAMS = psmqd_test
dist_check_SCRIPTS = psmq-progs.sh

psmqd_test_source = main.c topic-list.c psmqd.c cfg.c psmqd-startup.c psmq.c
psmqd_test_header = mtest.h psmqd-startup.h

psmqd_test_SOURCES = $(psmqd_test_source) $(psmqd_test_header)
psmqd_test_CFLAGS = -I$(top_srcdir)/inc \
	-I$(top_srcdir)/src \
	-I$(top_srcdir) \
	$(COVERAGE_CFLAGS)

psmqd_test_LDFLAGS = $(COVERAGE_LDFLAGS) -static
psmqd_test_LDADD = $(top_builddir)/src/libpsmqd.la \
	$(top_builddir)/lib/libpsmq.la

TESTS = $(check_PROGRAMS) $(dist_check_SCRIPTS)
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/tap-driver.sh
EXTRA_DIST = mtest.sh
CLEANFILES = tpsmqs.stderr tpsmqd.stderr tpsmqd.log tpsmqp.stdout \
	tpsmqs.stdout tpsmqd.stdout tpsmqp.stderr psmqd.log

# static code analyzer

if ENABLE_ANALYZER

analyze_plists = $(psmqd_test_source:%.c=%.plist)
MOSTLYCLEANFILES = $(analyze_plists)

$(analyze_plists): %.plist: %.c
	@echo "  CCSA  " $@
	@clang --analyze -DHAVE_CONFIG_H -I../inc -I../src -I.. -I. $< -o $@

analyze: $(analyze_plists)

endif
