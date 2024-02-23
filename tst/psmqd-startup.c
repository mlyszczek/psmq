/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
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

#include "psmqd-startup.h"

#include <embedlog.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "mtest.h"
#include "psmq.h"
#include "psmq-common.h"


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


int psmqd_main(int argc, char *argv[]);
mt_defs_ext();

struct main_args
{
	int     argc;
	char  **argv;
};


/* ==========================================================================
                                   _         __     __
              _   __ ____ _ _____ (_)____ _ / /_   / /___   _____
             | | / // __ `// ___// // __ `// __ \ / // _ \ / ___/
             | |/ // /_/ // /   / // /_/ // /_/ // //  __/(__  )
             |___/ \__,_//_/   /_/ \__,_//_.___//_/ \___//____/

   ========================================================================== */


pthread_t    gt_psmqd_t;                /* handler for psmqd_main() thread*/
char         gt_broker_name[QNAME_LEN]; /* current name of the used broker*/
struct psmq  gt_pub_psmq;               /* pub psmq for tests */
struct psmq  gt_sub_psmq;               /* sub psmq for tests */
char         gt_pub_name[QNAME_LEN];    /* pub queue name for tests */
char         gt_sub_name[QNAME_LEN];    /* sub queue name for tests */
struct psmq_msg gt_recvd_msg;           /* received message */


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
   ========================================================================== */


static void *psmqt_thread
(
	void             *arg     /* args to run psmqd_main() with */
)
{
	struct main_args *args;   /* args to run psmqd_main() with */
	int              *ret;    /* return value from psmqd_main() */
	int               i;      /* simple iterator */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	args = arg;

	ret = malloc(sizeof(*ret));
	*ret = psmqd_main(args->argc, args->argv);

	/* psmqd_main ended, free resources */
	for (i = 0; i != args->argc; ++i)
		free(args->argv[i]);

	free(args->argv);
	free(args);

	return ret;
}


/* ==========================================================================
    Creates thread that will run psmqd_main() with some default parameters.
    After starting thread, it waits some time to confirm daemon really did
    start.

    Returns 0 when psmqd_main() started, and -1 otherwise.
   ========================================================================== */


static int psmqt_run_default(void)
{
	int                i;      /* simple iterator */
	time_t             start;  /* starting point of waiting for confirmation */
	struct main_args  *args;   /* allocated args to create psmqd_main() with */
	struct timespec    tp;     /* time to sleep between psmqd_main() run check*/
	const char        *argv[] = { "psmqd", "-l6", "-p./psmqd.log",
		"-m10", NULL };
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	args = malloc(sizeof(*args));
	args->argc = sizeof(argv)/sizeof(*argv) - 1;
	args->argv = calloc(args->argc, sizeof(const char *));
	for (i = 0; i != args->argc; ++i)
	{
		args->argv[i] = malloc(strlen(argv[i]) + 1);
		strcpy(args->argv[i], argv[i]);
	}

	memset(gt_broker_name, 0x00, sizeof(gt_broker_name));
	strcpy(gt_broker_name, "/psmqd");
	mq_unlink(gt_broker_name);
	pthread_create(&gt_psmqd_t, NULL, psmqt_thread, args);

	/* wait until psmqd_start() creates gt_broker_name mqueue,
	 * that will be our signal that daemon started without
	 * problems */

	start = time(NULL);
	tp.tv_sec = 0;
	tp.tv_nsec = 1000 * 1000; /* 1[ms] */
	for (;;)
	{
		mqd_t mq;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


		mq = mq_open(gt_broker_name, O_RDONLY);
		if (mq != (mqd_t)-1)
		{
			/* file exists, that means
			 * psmqd_main() has created it and is
			 * running */
			mq_close(mq);
			return 0;
		}

		if (errno != ENOENT)
		{
			/* that error should not occur,
			 * something is very wrong
			 */
			el_perror(ELF, "mq_open() failed");
			return -1;
		}

		/* mqueue doesn't exist yet, psmqd_main()
		 * not running yes, we will be waiting
		 * maximum of 5 seconds before realizing
		 * psmqd probably crashed */
		if (time(NULL) - start >= 500)
		{
			/* psmqd couldn't create queue for more than 5
			 * seconds, it probably crashed or something */
			return -1;
		}

		/* psmqd still has time to create mqueue */
		nanosleep(&tp, NULL);
		continue;
	}
}


/* ==========================================================================
                       __     __ _          ____
        ____   __  __ / /_   / /(_)_____   / __/__  __ ____   _____ _____
       / __ \ / / / // __ \ / // // ___/  / /_ / / / // __ \ / ___// ___/
      / /_/ // /_/ // /_/ // // // /__   / __// /_/ // / / // /__ (__  )
     / .___/ \__,_//_.___//_//_/ \___/  /_/   \__,_//_/ /_/ \___//____/
    /_/
   ========================================================================== */


/* ==========================================================================
    Generates random alphanumeric data into 's'. 'l' number of bytes will
    be generated, including last '\0' terminator. So when l is 7, 6 random
    bytes will be stored into 's' and one '\0' at the end.
   ========================================================================== */


void psmqt_gen_random_string
(
	char              *s,  /* generated data will be stored here */
	size_t             l   /* length of the data to generate (with '\0') */
)
{
	static const char  alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	size_t             i;
	static int         inited;
	static char       *pad;
	static unsigned long long index;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	if (l == 0)
		return;

	if (inited == 0)
	{
		pad = malloc(1024);
		memset(pad, '-', 1024);
		inited = 1;
	}

#if TEST_ENABLE_RANDOM
	for (i = 0; i != l; ++i)
		s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
#else
	i = snprintf(s, l, "%llu", index++);
	if (i < l)
		strncat(s, pad, l - i);
#endif
	s[l - 1] = '\0';
}

/* ==========================================================================
    Generates random mqueue name, usefull when running multiple tests on one
    machine. Name will be generated with leading '/' character.  Won't
    generate name longer 32 chars
   ========================================================================== */


char * psmqt_gen_queue_name
(
	char              *s,  /* generated name will be stored here */
	size_t             l   /* length of the queue name */
)
{
	s[0] = '/';

	/* limit length of queue name */
	l = l > 32 ? 32 : l;

	/* generate unique queue name until it is different than
	 * gt_broker_name to prevent name clash */
	for (;;)
	{
		psmqt_gen_random_string(s + 1, l - 1);

		/* we are generating name for broker, so
		 * no need to check for clash */
		if (s == gt_broker_name)
			break;

		if (strcmp(s, gt_broker_name) != 0)
			break;
	}
	return s;
}


/* ==========================================================================
    generates 'array' with 'alen' unique queues names, each name of size
    'qlen'>
   ========================================================================== */


void psmqt_gen_unique_queue_name_array
(
	void    *array,  /* array to generate */
	size_t   alen,   /* number of elements in array */
	size_t   qlen    /* length of single string in array */
)
{
	size_t   i;      /* iterator */
	size_t   j;      /* yet another iterator */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	for (i = 0; i != alen; ++i)
	{
		char  *a;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		a = (char *)array + (i * qlen);
		psmqt_gen_queue_name(a, qlen);

		for (j = 0; j != i; ++j)
		{
			char  *b;
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


			b = (char *)array + (j * qlen);
			if (strcmp(a, b) == 0)
			{
				/* name clash! decrement i, and
				 * force another name generation */
				--i;
				break;
			}
		}
	}
}


/* ==========================================================================
   ========================================================================== */


void psmqt_prepare_test(void)
{
	mt_assert(psmqt_run_default() == 0);
}


/* ==========================================================================
   ========================================================================== */


void psmqt_prepare_test_with_clients(void)
{
	mt_assert(psmqt_run_default() == 0);
	psmqt_gen_queue_name(gt_pub_name, sizeof(gt_pub_name));

	/* prevent queue name clashes, generate name until it is
	 * different than pub queue name
	 */

	for (;;)
	{
		psmqt_gen_queue_name(gt_sub_name, sizeof(gt_sub_name));

		if (strcmp(gt_sub_name, gt_pub_name) != 0)
			break;
	}

	mt_fok(psmq_init_named(&gt_pub_psmq, gt_broker_name, gt_pub_name, 10));
	mt_fok(psmq_init_named(&gt_sub_psmq, gt_broker_name, gt_sub_name, 10));
	mt_fok(psmq_subscribe(&gt_sub_psmq, "/t"));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 's', 0, 0, "/t", NULL));
}


/* ==========================================================================
   ========================================================================== */


void psmqt_cleanup_test(void)
{
	int *ret;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	pthread_kill(gt_psmqd_t, SIGTERM);
	pthread_join(gt_psmqd_t, (void **)&ret);
	mq_unlink(gt_broker_name);
	mt_fail(*ret == 0);
	free(ret);
}


/* ==========================================================================
   ========================================================================== */


void psmqt_cleanup_test_with_clients(void)
{
	int *ret;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	pthread_kill(gt_psmqd_t, SIGTERM);
	pthread_join(gt_psmqd_t, (void **)&ret);
	mq_unlink(gt_broker_name);
	mt_fail(*ret == 0);
	free(ret);

	mt_fok(psmq_cleanup(&gt_pub_psmq));
	mt_fok(psmq_cleanup(&gt_sub_psmq));
	mq_unlink(gt_pub_name);
	mq_unlink(gt_sub_name);
}


/* ==========================================================================
   ========================================================================== */


int psmqt_receive_expect
(
	struct psmq     *psmq,
	char             cmd,
	unsigned char    data,
	unsigned short   paylen,
	const char      *topic,
	void            *payload
)
{
	int              e;
	struct psmq_msg  msg;
	int              topiclen;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	memset(&msg, 0x00, sizeof(msg));
	if (psmq_receive(psmq, &msg) == -1)
		return -1;

	topiclen = 0;
	if (strchr("spu", cmd))
		topiclen = strlen(msg.data) + 1;

	e = 0;

	e |= msg.ctrl.cmd != cmd;
	e |= msg.ctrl.data != data;
	e |= msg.paylen != paylen;
	if (topic)
		e |= strcmp(msg.data, topic);
	if (paylen == 0 && topiclen == 0)
		e |= msg.data[0] != '\0';
	if (msg.paylen && paylen && payload)
		e |= memcmp(msg.data + topiclen, payload, msg.paylen);

	return e;
}
