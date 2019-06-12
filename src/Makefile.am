#include ../Makefile.am.coverage

psmqd_source = cfg.c globals.c psmqd.c broker.c topic-list.c
psmqs_source = psmq-sub.c
psmqp_source = psmq-pub.c
psmq_headers = cfg.h broker.h globals.h topic-list.h $(top_srcdir)/valid.h \
	$(top_srcdir)/psmq-common.h

bin_cflags = $(COVERAGE_CFLAGS) -I$(top_srcdir) -I$(top_srcdir)/inc
bin_ldflags = $(COVERAGE_LDFLAGS)

if ENABLE_STANDALONE

bin_PROGRAMS = psmqd psmq-pub psmq-sub
standalone_cflags = -DPSMQ_STANDALONE=1

psmqd_SOURCES = $(psmqd_source) $(psmq_headers)
psmqd_LDFLAGS = $(bin_ldflags)
psmqd_CFLAGS = $(bin_cflags) $(standalone_cflags)

psmq_pub_SOURCES = $(psmqp_source)
psmq_pub_LDFLAGS = $(bin_ldflags)
psmq_pub_CFLAGS = $(bin_cflags) $(standalone_cflags)
psmq_pub_LDADD = $(top_builddir)/lib/libpsmq.la

psmq_sub_SOURCES = $(psmqs_source)
psmq_sub_LDFLAGS = $(bin_ldflags)
psmq_sub_CFLAGS = $(bin_cflags) $(standalone_cflags)
psmq_sub_LDADD = $(top_builddir)/lib/libpsmq.la

endif # ENABLE_STANDALONE

if ENABLE_LIBRARY

lib_LTLIBRARIES = libpsmqd.la
library_cflags = -DPSMQ_LIBRARY=1

libpsmqd_la_SOURCES = $(psmqd_source) $(psmqs_source) $(psmqp_source)
libpsmqd_la_CFLAGS = $(bin_cflags) $(library_cflags)
libpsmqd_la_LDFLAGS = $(bin_ldflags) -version-info 1:1:1

endif # ENABLE_LIBRARY

# static code analyzer

if ENABLE_ANALYZER

analyze_plists = $(psmqd_source:%.c=%.plist)
analyze_plists += $(psmqp_source:%.c=%.plist)
analyze_plists += $(psmqs_source:%.c=%.plist)
MOSTLYCLEANFILES = $(analyze_plists)

$(analyze_plists): %.plist: %.c
	@echo "  CCSA  " $@
	@clang --analyze -DHAVE_CONFIG_H -I../inc -I.. $< -o $@

analyze: $(analyze_plists)

endif
