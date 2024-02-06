/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef PSMQ_EMBEDLOG_MOCK_H
#define PSMQ_EMBEDLOG_MOCK_H

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

/* for systems that don't have embedlog - or they have choosen not to compile
 * it, we provide simple mock functions. This will still allow for program
 * to print log messages, though in limited fassion. Better than nothing */

/* Disable init, cleanup and option functions, they are useless, and logging
 * system should already be in place by the time psmq starts */

#define el_oinit(...)
#define el_ocleanup(...)
#define el_ooption(...)

/* since we are printing everything to stderr, we don't really need flush */
#define el_oflush(...)

/* Sadly, el_pmemory() is not trivial function, so no pretty data dump for
 * heretics */
#define el_opmemory(...)

/* el_print() is almost equivalent of standard printf(), so that's easy */
#define el_oprint(UNUSED, fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

/* el_perror() is equivalent to perror(), but unlike perror() it can also
 * accept fmt and not simple char *. It's trickier, but simple enough for
 * it to be implemented with static inline function */
#define el_operror(UNUSED, fmt, ...) el_operror_mock(fmt "\n", ##__VA_ARGS__)

static inline int el_operror_mock
(
	const char    *fmt,    /* message format (see printf (3)) */
	               ...     /* additional parameters for fmt */
)
{
	int            rc;     /* return code from vfprintf() */
	va_list        ap;     /* argument pointer for variadic variables */
	unsigned long  e;      /* last errno value we will be printing */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	e = errno;
	rc = 0;

	if (fmt)
	{
		/* print message only when user provided any message */
		va_start(ap, fmt);
		rc |= vfprintf(stderr, fmt, ap);
		va_end(ap);
	}

	/* print last errno */
	rc |= fprintf(stderr, "errno num: %lu, strerror: %s\n", e, strerror(e));

	return rc;
}

#endif /* PSMQ_EMBEDLOG_MOCK_H */
