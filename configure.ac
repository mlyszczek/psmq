AC_INIT([psmq], [9999], [michal.lyszczek@bofc.pl])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_HEADERS([psmq-config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC
AC_PROG_LIBTOOL
LT_INIT
AC_CANONICAL_HOST
AC_CONFIG_FILES([Makefile www/Makefile lib/Makefile src/Makefile \
                 tst/Makefile inc/Makefile man/Makefile])
AC_CONFIG_LINKS([tst/mtest.sh:tst/mtest.sh])
AC_CHECK_HEADERS([linux/limits.h])

LDFLAGS="$LDFLAGS -L/usr/local/lib -L/usr/lib"
CFLAGS="$CFLAGS -I/usr/local/include -I/usr/include"

m4_include([m4/gcov.m4])
AC_TDD_GCOV
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_CXXFLAGS)
AC_SUBST(COVERAGE_LDFLAGS)

AX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

AC_SEARCH_LIBS([el_init], [embedlog])
AC_SEARCH_LIBS([mq_open], [rt])
AC_CONFIG_FILES(inc/psmq.h)
# POSIX mandates signals to be implemented *always*, but still there are
# some super tiny unixes that decided not to implement them. Also, embedded,
# remember? This is mainly for embedded which may not implement POSIX at all.
# But if programs are build with autotools, we can be fairy sure it has signals.
AC_DEFINE_UNQUOTED([PSMQ_NO_SIGNALS], [0], [Disable signals handling])
# embedlog is not strictly neccessary for this library to work, it allows for
# logging to file and pretty print of logs. While additional library is not
# a problem on Linux and other Unices it may be a problem on embedded systems,
# that do not want to waste additional resources on something they don't need.
# When using autotools, we force embedlog because it's not a problem, and
# this can be defined to 0 on embedded.
AC_DEFINE_UNQUOTED([PSMQ_HAVE_EMBEDLOG], [1], [Disable or enable embedlog logging])

###
# solaris has some serious design problem, since we enabled POSIX
# >=200112, solaris will not let use c89 compiler even though we
# are c89 compatible (with some header extension) So force c99 on
# solaris sytem.
#

case ${host_os} in
    solaris*)
        CFLAGS="-std=c99 ${CFLAGS}"
        ;;
esac


###
# --enable-analyzer
#

AC_ARG_ENABLE([analyzer],
    AS_HELP_STRING([--enable-analyzer], [Enable static code analyzer]),
    [enable_analyzer="yes"], [enable_analyzer="no"])
AM_CONDITIONAL([ENABLE_ANALYZER], [test "x$enable_analyzer" = "xyes"])


###
# --enable-standalone
#


AC_ARG_ENABLE([standalone],
    AS_HELP_STRING([--enable-standalone], [Create standalone binaries]),
    [], [enable_standalone="yes"])

AM_CONDITIONAL([ENABLE_STANDALONE], [test "x$enable_standalone" = "xyes"])
AS_IF([test "x$enable_standalone" = "xyes"],
[
    AC_DEFINE([ENABLE_STANDALONE], [1], [Create standalone binaries])
],
# else
[
    enable_standalone="no"
])


###
# --enable-library
#


AC_ARG_ENABLE([library],
    AS_HELP_STRING([--enable-library], [Create libraries from programs]),
    [], [enable_library="yes"])

AM_CONDITIONAL([ENABLE_LIBRARY], [test "x$enable_library" = "xyes"])
AS_IF([test "x$enable_library" = "xyes"],
[
    AC_DEFINE([ENABLE_LIBRARY], [1], [Create libraries from programs])
],
# else
[
    enable_library="no"
])


###
# VARIABLES=value options
#

###
# PSMQ_MAX_CLIENTS
#


AC_ARG_VAR([PSMQ_MAX_CLIENTS], [Maximum clients in broker])
AS_IF([test "x$PSMQ_MAX_CLIENTS" = x], [PSMQ_MAX_CLIENTS="128"])
AC_DEFINE_UNQUOTED([PSMQ_MAX_CLIENTS], [$PSMQ_MAX_CLIENTS], [Maximum clients in broker])

AS_IF([test $PSMQ_MAX_CLIENTS -lt 2],
[
    AC_MSG_ERROR(PSMQ_MAX_CLIENTS must be at least 2)
])

###
# PSMQ_MSG_MAX
#


AC_ARG_VAR([PSMQ_MSG_MAX], [Maximum length of message (topic + payload)])
AS_IF([test "x$PSMQ_MSG_MAX" = x], [PSMQ_MSG_MAX="255"])

# reason for this limit is documented in inc/psmq.h.in
AS_IF([test $PSMQ_MSG_MAX -lt 6],
[
    AC_MSG_ERROR(PSMQ_MSG_MAX must be at least 6)
])

AC_OUTPUT

echo
echo "psmq compilation configuration summary"
echo
echo "build standalone.........: $enable_standalone"
echo "build library............: $enable_library"
echo ""
echo "max clients............. : $PSMQ_MAX_CLIENTS"
echo "max message size........ : $PSMQ_MSG_MAX"
