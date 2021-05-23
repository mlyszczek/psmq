/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         -------------------------------------------------------------
        / helper program that allows subscribing and printing message \
        \ from specified broker                                       /
         -------------------------------------------------------------
                                                       /
                                                      /
                  oO)-.                       .-(Oo
                 /__  _\                     /_  __\
                 \  \(  |     ()~()         |  )/  /
                  \__|\ |    (-___-)        | /|__/
                  '  '--'    ==`-'==        '--'  '
   ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#ifdef HAVE_CONFIG_H
#   include "psmq-config.h"
#endif

#include <ctype.h>
#include <embedlog.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if PSMQ_NO_SIGNALS == 0
#   include <signal.h>
#endif

#include "psmq.h"
#include "psmq-common.h"


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


#define EL_OPTIONS_OBJECT &psmqs_log
static struct el psmqs_log;
static struct el psmqs_out;
static int run;
static int flush;


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    Signal handler for all signals
   ========================================================================== */

#if PSMQ_NO_SIGNALS == 0

static void sigint_handler
(
	int signo   /* signal that triggered this handler */
)
{
	(void)signo;

	switch (signo)
	{
	case SIGUSR1:
		flush = 1;
		return;

	case SIGTERM:
	case SIGINT:
		run = 0;
		return;
	}
}

#endif


/* ==========================================================================
    Check whether payload is binary data or not. It's treated as binary when
    at least one byte is non-printable character.
   ========================================================================== */


static int is_payload_binary
(
	unsigned char  *payload,
	unsigned short  paylen
)
{
	unsigned short  i;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	/* treat no data as binary data,
	 * paylen == 1 could be '\0' but
	 * we cannot treat is as text with
	 * 100% certainty */
	if (paylen == 0 || paylen == 1)
		return 1;

	/* check all but the very last character */
	for (i = 0; i != paylen - 1; ++i)
		if (!isprint(payload[i]) && !isspace(payload[i]))
			return 1;

	/* if last string is null, we are dealing
	 * with proper string, data NOT binary */
	if (payload[i] == '\0')
		return 0;

	/* something else got, even if this is
	 * printable character we cannot use it
	 * to print it as string as there is no
	 * null terminator. And anyway, string
	 * without null terminator is not a
	 * string. */
	return 1;
}


/* ==========================================================================
    Called by us when we receive message from broker.
   ========================================================================== */


static int on_receive
(
	struct psmq_msg  *msg,      /* full received message */
	char             *topic,    /* topic of received message */
	unsigned char    *payload,  /* message payload */
	unsigned short    paylen,   /* length of payload data */
	unsigned int      prio      /* message priority */
)
{
	unsigned short    timeout;  /* timeout value from broker reply */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	switch (msg->ctrl.cmd)
	{
		case PSMQ_CTRL_CMD_CLOSE:
			el_oprint(OELN, "broker has closed the connection");
			errno = msg->ctrl.data;
			return -1;

		case PSMQ_CTRL_CMD_IOCTL:
			memcpy(&timeout, payload + 1, sizeof(timeout));
			el_oprint(OELN, "reply timeout set %hu", timeout);
			return 0;

		case PSMQ_CTRL_CMD_PUBLISH:
			if (is_payload_binary(payload, paylen))
			{
				el_oprint(ELN, &psmqs_out, "p:%u %s data(%hu)",
						prio, topic, paylen);
				el_opmemory(ELN, &psmqs_out, payload, paylen);
			}
			else
			{
				el_oprint(ELN, &psmqs_out, "p:%u %s data(%4hu): %s",
						prio, topic, paylen, payload);
			}

			return 0;

		default:
			el_oprint(ELE, &psmqs_out, "Unknown cmd received: %c(%02x)",
					msg->ctrl.cmd, msg->ctrl.cmd);
			return -1;
	}
}


/* ==========================================================================
                                              _
                           ____ ___   ____ _ (_)____
                          / __ `__ \ / __ `// // __ \
                         / / / / / // /_/ // // / / /
                        /_/ /_/ /_/ \__,_//_//_/ /_/

   ========================================================================== */


#if PSMQ_STANDALONE
int main
#else
int psmq_sub_main
#endif
(
	int               argc,    /* number of arguments in argv */
	char             *argv[]   /* arguments from command line */
)
{
	int               arg;     /* arg for getopt() */
	struct psmq       psmq;    /* psmq object */
	const char       *qname;   /* name of the client queue */
	int               got_b;   /* -b option was passed */
	int               got_t;   /* -t option was passed */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


#if PSMQ_NO_SIGNALS == 0
	{
		struct sigaction  sa;  /* signal action instructions */
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


		/* install signal handler to nicely exit program */
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = sigint_handler;
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		sigaction(SIGUSR1, &sa, NULL);
	}
#endif


	el_oinit(&psmqs_log);
	el_oinit(&psmqs_out);
	el_ooption(&psmqs_out, EL_OUT, EL_OUT_STDOUT);
	el_ooption(&psmqs_out, EL_FILE_SYNC_EVERY, 0);
	el_ooption(&psmqs_out, EL_TS, EL_TS_LONG);
	el_ooption(&psmqs_out, EL_TS_TM, EL_TS_TM_REALTIME);
	el_ooption(&psmqs_out, EL_PRINT_LEVEL, 0);

	got_b = 0;
	got_t = 0;
	flush = 0;
	run = 1;
	qname = "/psmq-sub";
	memset(&psmq, 0x00, sizeof(psmq));
	optind = 1;

	while ((arg = getopt(argc, argv, ":hvt:b:n:o:")) != -1)
	{
		struct psmq_msg  msg;  /* control message recieved from broker */
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


		switch (arg)
		{
		case 'n': qname = optarg; break;

		case 'b':
			/* broker name passed, open connection to the broker,
			 * if qname was not set, use default /psmq-sub queue */
			got_b = 1;
			el_oprint(OELN, "init: broker name: %s, queue name: %s",
					optarg, qname);
			if (psmq_init(&psmq, optarg, qname, 10) != 0)
			{
				switch (errno)
				{
				case EINVAL:
					el_oprint(OELF, "broker or queue name is invalid");
					break;

				case ENAMETOOLONG:
					el_oprint(OELF, "queue name is too long (%lu), max is %u",
							strlen(qname), PSMQ_MSG_MAX - 1);
					break;

				case ENOENT:
					el_oprint(OELF, "broker %s doesn't exist", optarg);
					break;

				default:
					el_operror(OELF, "psmq_init: unknown error: %d", errno);
					break;
				}

				return 1;
			}
			el_oprint(OELN, "connected to broker %s", optarg);
			break;

		case 't':
			/* topic passed, subscribe to the broker */
			got_t = 1;
			if (psmq_subscribe(&psmq, optarg) != 0)
			{
				switch (errno)
				{
				case EBADF:
					el_oprint(OELF,
							"subscribe failed, was -b set before -t option?");
					break;

				case ENOBUFS:
					el_oprint(OELF,
							"subscribe failed, topic %s is too long", optarg);
					break;

				case EBADMSG:
					el_oprint(OELF,
							"subscribe failed, topic %s is invalid", optarg);
					break;

				default:
					el_operror(OELF, "subscribe: unknown error: %d", errno);
					break;
				}

				psmq_cleanup(&psmq);
				return 1;
			}

			if (psmq_receive(&psmq, &msg, NULL) != 0)
			{
				el_operror(OELF, "error reading from queue");
				psmq_cleanup(&psmq);
				return 1;
			}

			if (msg.ctrl.cmd != PSMQ_CTRL_CMD_SUBSCRIBE)
			{
				el_oprint(OELF, "invalid reply from broker, cmd: %02x",
						msg.ctrl.cmd);
				psmq_cleanup(&psmq);
				return 1;
			}

			if (msg.ctrl.data == EBADMSG)
			{
				el_oprint(OELF, "subscribe failed, topic %s is invalid",
						msg.data);
				psmq_cleanup(&psmq);
				return 1;
			}

			el_oprint(OELN, "subscribed to: %s", msg.data);
			break;

		case 'o':
			el_ooption(&psmqs_out, EL_OUT, EL_OUT_FILE);
			el_ooption(&psmqs_out, EL_FILE_SYNC_EVERY, 32767);

			if (el_ooption(&psmqs_out, EL_FPATH, optarg) != 0)
			{
				el_operror(OELF, "failed to open file %s for logging", optarg);
				psmq_cleanup(&psmq);
				return 1;
			}

			break;

		case 'v':
			printf("%s v"PACKAGE_VERSION"\n"
					"by Michał Łyszczek <michal.lyszczek@bofc.pl>\n", argv[0]);
			return 0;

		case 'h':
			printf(
					"%s - listen to subscribed messages over psmq\n"
					"\n"
					"usage: \n"
					"\t%s [-h | -v]\n"
					"\t%s <[-n mqueue-name]> <-b name> <-t topic> <[-t topic]> [-o <file>]\n"
					"\n", argv[0], argv[0], argv[0]);
			printf(
					"\t-h                   shows help and exit\n"
					"\t-v                   shows version and exit\n"
					"\t-n <mqueue-name>     mqueue name to use by sub to receive data from broker\n"
					"\t                     if not specified, default /psmq-sub will be used\n"
					"\t-b <broker-name>     name of the broker (with leading '/' - like '/qname')\n"
					"\t-t <topic>           topic to subscribe to, can be used multiple times\n"
					"\t-o <file>            file where to store logs from incoming messages\n"
					"\t                     if not set, stdout will be used\n");
			return 0;

		case ':':
			el_oprint(OELF, "option -%c requires an argument", optopt);
			return 1;

		case '?':
			el_oprint(OELF, "unknown option -%c", optopt);
			return 1;
		}
	}

	if (got_b == 0)
	{
		/* no -b means no psmq_init() has been called,
		 * we can bail without cleaning */
		el_oprint(OELF, "missing -b option");
		return 1;
	}

	if (got_t == 0)
	{
		el_oprint(OELF, "missing -t option");
		psmq_cleanup(&psmq);
		mq_unlink(qname);
		return 1;
	}

	if (psmq_ioctl(&psmq, PSMQ_IOCTL_REPLY_TIMEOUT, 100) != 0)
		el_operror(OELW, "failed to set reply timeout, data might be lost");

	el_oprint(OELN, "start receiving data");

	while (run)
	{
		struct psmq_msg  msg;  /* buffer to receive message from boker */
		unsigned int     prio; /* received message priority */
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


		if (psmq_receive(&psmq, &msg, &prio) != 0)
		{
			if (flush)
			{
				el_oflush(&psmqs_out);
				flush = 0;
				continue;
			}

			if (errno == EINTR)
			{
				el_oprint(OELN, "interrupt received, exit");
				break;
			}

			el_operror(OELF, "psmq_receive() failed");
			break;
		}

		if (on_receive(&msg, PSMQ_TOPIC(msg), PSMQ_PAYLOAD(msg),
					msg.paylen, prio) == -1)
			break;
	}

	psmq_cleanup(&psmq);
	mq_unlink(qname);
	return 0;
}
