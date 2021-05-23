lib_LTLIBRARIES = libpsmq.la

libpsmq_la_SOURCES = psmq.c ../utils.c

libpsmq_la_LDFLAGS = $(COVERAGE_LDFLAGS) \
		-version-info 2:0:2
#		       .------' | `---.
#		    current:revision:age
#		       |        |     `- increment if interfaces have been added
#		       |        |        set to zero if interfaces have been removed
#		       |        |        or changed
#		       |        `- increment if source code has changed
#		       |           set to zero if current is incremented
#		       `- increment if interfaces have been added, removed or changed
libpsmq_la_CFLAGS = $(COVERAGE_CFLAGS) -I$(top_srcdir)/inc -I$(top_srcdir) \
	-I$(top_builddir)/inc

# static code analyzer

if ENABLE_ANALYZER

analyze_plists = $(libpsmq_la_SOURCES:%.c=%.plist)
MOSTLYCLEANFILES = $(analyze_plists)

$(analyze_plists): %.plist: %.c
	@echo "  CCSA  " $@
	@clang --analyze -DHAVE_CONFIG_H -I../inc -I.. $< -o $@

analyze: $(analyze_plists)

endif
