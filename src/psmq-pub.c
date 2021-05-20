/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         ------------------------------------------------------------
        / helper program that allows publishing message to specified \
        \ broker from command line or script                         /
         ------------------------------------------------------------
             \
              \
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

#include "psmq.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#if PSMQ_NO_SIGNALS == 0
#include <signal.h>
#endif

#include "psmq-common.h"


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    Need this stub to handle SIGINT and SIGTERM
   ========================================================================== */

#if PSMQ_NO_SIGNALS == 0

static void sigint_handler(int signo) { (void)signo; }

#endif


/* ==========================================================================
    Publishes single messages with 'payload' on 'topic' to 'psmq'. Returns
    0 on success, and -1 on error.
   ========================================================================== */


static int publish
(
	struct psmq  *psmq,     /* psmq object */
	const char   *topic,    /* topic of the message */
	const char   *payload,  /* payload of the message */
	size_t        paylen,   /* message length */
	unsigned int  prio      /* message priority */
)
{
	/* message set in program arguments, simply
	 * send message and exit */

	if (psmq_publish(psmq, topic, payload, paylen, prio) == 0)
		return 0;

	switch (errno)
	{
	case EINVAL:
		fprintf(stderr, "f/failed to publish, invalid prio %u\n", prio);
		break;

	case EBADMSG:
		fprintf(stderr, "f/failed to publish to %s invalid topic\n",
				topic);
		break;

	case ENOBUFS:
		fprintf(stderr, "f/topic or message is too long\n");
		break;

	default:
		fprintf(stderr, "f/psmq_publish: unknown error: %d", errno);
		break;
	}

	return -1;
}


/* ==========================================================================
    Sends whatever shows on the stdin on 'topic' to 'psmq' broker. Invokes
    one psmq_publish() per read line.
   ========================================================================== */


static void send_stdin
(
	struct psmq  *psmq,      /* psmq object */
	const char   *topic,     /* topic of the message */
	unsigned int  prio       /* message priority */
)
{
	unsigned int  topiclen;  /* length of topic string */
	unsigned int  linemax;   /* max allowed length of line buf */
	unsigned int  paylen;    /* length of payload to send */
	char          line[PSMQ_MSG_MAX];  /* single line read from stdin */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	topiclen = strlen(topic);
	if (topiclen >= sizeof(line))
	{
		fprintf(stderr, "f/topic is too long, max is %lu\n",
				(unsigned long)sizeof(line) - 1);
		return;
	}
	/* internal psmq buffer of size PSMQ_MSG_MAX
	 * shares space between topic and data, so
	 * line + topic cannot exceed that size */
	linemax = sizeof(line) - (topiclen + 1);

	/* message was not set, so we take a little
	 * longer path, read stdin until EOF is
	 * encoutered and send each line in separate
	 * line */
	for (;;)
	{
		/* set last byte of line buffer to
		 * something other than '\0' to know
		 * whether fgets overwrite it or not */
		line[linemax - 1] = 0x55;
		if (fgets(line, linemax, stdin) == NULL)
		{
			/* if end of file is reached and no
			 * data has been read, we're done */
			if (feof(stdin))
				return;

			perror("f/error reading stdin");
			return;
		}

		if (line[linemax - 1] == '\0' && line[linemax - 2] != '\n')
		{
			/* fgets did overwrite last byte with
			 * '\0' which means it filled whole
			 * line buffer
			 *
			 * and
			 *
			 * last character in string is not new line
			 *
			 * which means, user provided line
			 * that is too long to fit into one
			 * message, abort with error
			 */
			fprintf(stderr, "f/line is too long, max line is %u\n", linemax - 2);
			return;
		}

		/* now we have full line in buffer, ship it. */
		paylen = strlen(line);
		/* remove new line character from string */
		line[paylen - 1] = '\0';
		/* send valid string with null terminator */
		if (publish(psmq, topic, line, paylen, prio) != 0)
			return;
	}
}


/* ==========================================================================
    Sends whatever shows on the stdin on 'topic' to 'psmq' broker. Invokes
    one psmq_publish() per PSMQ_MSG_MAX - strlen(topic) bytes read, or less
    when stdin ends.  Basically works as a stream, when stdin message is
    larger than PSMQ_MSG_MAX, it will be split into multiple psmq messages.

    Since it is impossible to read stdin in binary mode in portable way, we
    won't be hacking it with freopen() as it's platform dependant anyway,
    and will use posix read() straight away as it's simpler and better for
    that purpose. And nobody cares about Windows anyway.
   ========================================================================== */


static void send_stdin_binary
(
	struct psmq  *psmq,      /* psmq object */
	const char   *topic,     /* topic of the message */
	unsigned int  prio       /* message priority */
)
{
	unsigned int  topiclen;  /* length of topic string */
	ssize_t       r;         /* value returned from read() */
	char          data[PSMQ_MSG_MAX];  /* single data packet read from stdin */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	topiclen = strlen(topic) + 1 /* null character is also send with topic */;

	for (;;)
	{
		r = read(STDIN_FILENO, data, sizeof(data) - topiclen);

		if (r == -1)
		{
			fprintf(stderr, "Failed to read from stdin: %s (%d)\n",
					strerror(errno), errno);
			return;
		}

		/* end of file reached, nothing more to send */
		if (r == 0)
			return;

		/* send over psmq what has been read */
		if (publish(psmq, topic, data, r, prio) != 0)
			return;

		/* there is probably more data to send, continue */
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
int psmq_pub_main
#endif
(
	int          argc,         /* number of arguments in argv */
	char        *argv[]        /* arguments from command line */
)
{
	int          arg;          /* arg for getopt() */
	int          send_empty;   /* flag: send empty message on topic */
	int          send_binary;  /* flag: send binary data from stdin */
	unsigned int prio;         /* message priority */
	const char  *broker_name;  /* queue name of the broker */
	const char  *qname;        /* name of the client queue */
	const char  *message;      /* single message to send (-m parameter) */
	const char  *topic;        /* topic to send message to (-t parameter) */
	struct psmq  psmq;         /* psmq object */
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
	}
#endif


	broker_name = NULL;
	message = NULL;
	topic = NULL;
	qname = NULL;
	send_empty = 0;
	send_binary = 0;
	prio = 0;

	/* read input arguments */
	optind = 1;
	while ((arg = getopt(argc, argv, ":hvet:b:Bm:n:p:")) != -1)
	{
		switch (arg)
		{
		case 'b': broker_name = optarg; break;
		case 'm': message = optarg; break;
		case 't': topic = optarg; break;
		case 'n': qname = optarg; break;
		case 'p': prio = atoi(optarg); break;
		case 'e': send_empty = 1; break;
		case 'B': send_binary = 1; break;
		case 'v':
			printf("%s v"PACKAGE_VERSION"\n"
					"by Michał Łyszczek <michal.lyszczek@bofc.pl>\n", argv[0]);

			return 0;

		case 'h':
			printf(
					"%s - publish message over psmq\n"
					"\n"
					"usage: \n"
					"\t%s [-h | -v]\n"
					"\t%s -b <name> -t <topic> [-m <message> | -e | -B] "
							"[-n <mqueue-name>] [-p <prio>]"
					"\n", argv[0], argv[0], argv[0]);
			printf("\n"
					"\t-h               print this help and exit\n"
					"\t-v               print version and exit\n"
					"\t-b <name>        name of the broker (with leading '/' - like '/qname')\n"
					"\t-t <topic>       topic on which message should be published\n"
					"\t-m <message>     message to publish, if not set read from stdin\n"
					"\t-e               publish message without payload on topic\n"
					"\t-B               publish stdin read in binary mode\n"
					"\t-n <mqueue-name> mqueue name to use by pub to receive data from broker\n"
					"\t                 if not set, default /psmq_pub will be used\n"
					"\t-t <prio>        message priority, must be int, default: 0\n");
			printf("\n"
					"When message is read from stdin, program will send each line as separate\n"
					"message on passed topic until EOF is encoutered\n");

			return 0;

		case ':':
			fprintf(stderr, "f/option -%c requires an argument\n", optopt);
			return 1;

		case '?':
			fprintf(stderr, "f/unknown option -%c\n", optopt);
			return 1;
		}
	}

	/* validate arguments */

	if (broker_name == NULL)
	{
		fprintf(stderr, "f/missing broker name (-b) option\n");
		return 1;
	}

	if (topic == NULL)
	{
		fprintf(stderr, "f/missing topic (-t) option\n");
		return 1;
	}

	if ((!!message + send_binary + send_empty) > 1)
	{
		fprintf(stderr, "f/only one of -m, -e and -B can be used\n");
		return 1;
	}

	/* if queue name not set, use default */
	if (qname == NULL)
		qname = "/psmq_pub";

	/* now the action can start */
	if (psmq_init(&psmq, broker_name, qname, 2) != 0)
	{
		switch(errno)
		{
		case ENOENT:
			fprintf(stderr, "f/broker %s doesn't exist\n", broker_name);
			break;

		case EINVAL:
			fprintf(stderr, "f/broker or queue name is invalid\n");
			break;

		case ENAMETOOLONG:
			fprintf(stderr, "f/queue name is too long (%lu), max is %u\n",
					(unsigned long)strlen(qname), PSMQ_MSG_MAX - 1);
			break;

		default:
			fprintf(stderr, "f/psmq_init: unknown error: %d", errno);
		}

		return 1;
	}

	if (send_empty)
		publish(&psmq, topic, NULL, 0, prio);
	else if (send_binary)
		send_stdin_binary(&psmq, topic, prio);
	else if (message)
		publish(&psmq, topic, message, strlen(message) + 1, prio);
	else
		send_stdin(&psmq, topic, prio);

	psmq_cleanup(&psmq);
	mq_unlink(qname);
	return 0;
}
