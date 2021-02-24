lib_LTLIBRARIES = libpsmq.la

libpsmq_la_SOURCES = psmq.c ../utils.c

libpsmq_la_LDFLAGS = -version-info 1:0:1 $(COVERAGE_LDFLAGS)
libpsmq_la_CFLAGS = $(COVERAGE_CFLAGS) -I$(top_srcdir)/inc -I$(top_srcdir)

# static code analyzer

if ENABLE_ANALYZER

analyze_plists = $(libpsmq_la_SOURCES:%.c=%.plist)
MOSTLYCLEANFILES = $(analyze_plists)

$(analyze_plists): %.plist: %.c
	@echo "  CCSA  " $@
	@clang --analyze -DHAVE_CONFIG_H -I../inc -I.. $< -o $@

analyze: $(analyze_plists)

endif
