/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

/* ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */

#ifdef HAVE_CONFIG_H
#   include "psmq-config.h"
#endif

#include <embedlog.h>
#include <errno.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "psmqd-startup.h"
#include "mtest.h"
#include "psmq.h"
#include "topic-list.h"
#include "psmq-common.h"


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


mt_defs_ext();

struct wildcard_matches
{
	const char    *topic;
	unsigned char  matches[64];
};

struct multi_ps
{
	int  num_pub;
	int  num_sub;
};

struct multi_ps_targs
{
	struct psmqd_tl **tl;       /* list with published/received data */
	struct psmq      *psmq;     /* psmq client object */
	int               num_msg;  /* how many messages to publish */
	int               id;       /* id of the pub thread */
};

/* Function exported from libpsmq, even tho it is exported publicly
 * it's declaration can't be found in include files and is intended
 * for internal use only, like in tests. */
int psmq_publish_msg(struct psmq *psmq, char cmd, unsigned char data,
	const char *topic, const void *payload, size_t paylen, unsigned int prio);

/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    This threads reads data from passed psmq client and puts received data
    into passed list until broker closes connection.
   ========================================================================== */


static void *multi_sub_thread
(
	void                   *arg    /* thread argument */
)
{
	struct multi_ps_targs  *args;  /* thread argument */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	args = arg;

	for (;;)
	{
		struct psmqd_tl **tl;       /* list where to put data onto */
		struct psmq_msg   msg;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		tl = args->tl;
		memset(&msg, 0x00, sizeof(msg));

		if (psmq_receive(args->psmq, &msg) == -1)
			return NULL;

		/* we are being told to stop, so stop */
		if (strcmp(msg.data, "/s") == 0)
			return NULL;

		/* transform "topic\0payload" into
		 * "topic payload" format */
		msg.data[strlen(msg.data)] = ' ';

		/* add data to a list */
		if (psmqd_tl_add(tl, msg.data) != 0)
			return NULL;
	}
}


/* ==========================================================================
    Sends random data on topic based on thread id (id == 0 -> topic == /1),
    num_msg times and stores sent data into list.
   ========================================================================== */


static void *multi_pub_thread
(
	void                   *arg    /* thread argument */
)
{
	struct multi_ps_targs  *args;  /* thread argument */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	args = arg;

	for (; args->num_msg >= 0; --args->num_msg)
	{
		char    data[PSMQ_MSG_MAX];
		size_t  paylen;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		/* data buffer contains 2 fields, "TOPIC\0DATA" */
		data[0] = '/';
		data[1] = '1' + args->id;
		data[2] = '\0';
		paylen = rand() % (PSMQ_MSG_MAX - 5 + 1);

		data[3] = '\0';
		psmqt_gen_random_string(data + 3, paylen);

		if (psmq_publish(args->psmq, data, data + 3, paylen) != 0)
			return NULL;

		/* now change '\0' in topic into ' ' and we have nice
		 * buffer to put onto list */
		data[2] = ' ';
		if (psmqd_tl_add(args->tl, data) != 0)
			return NULL;
	}

	return NULL;
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_wildard_test
(
	const char              **sub_topics,
	size_t                    sub_topics_num,
	struct wildcard_matches  *matches,
	size_t                    matches_num
)
{
	struct psmq              *sub_psmq;
	struct psmq               pub_psmq;
	char                    (*sub_qname)[QNAME_LEN];
	char                     *pub_qname;
	size_t                    i;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	sub_psmq = malloc(sub_topics_num * sizeof(*sub_psmq));
	sub_qname = malloc((sub_topics_num + 1) * sizeof(*sub_qname));

	psmqt_gen_unique_queue_name_array(sub_qname, sub_topics_num + 1, QNAME_LEN);
	pub_qname = sub_qname[sub_topics_num];

	mt_fok(psmq_init_named(&pub_psmq, gt_broker_name, pub_qname, 4));
	for (i = 0; i != sub_topics_num; ++i)
	{
		mt_fok(psmq_init_named(&sub_psmq[i], gt_broker_name, sub_qname[i], 4));
		mt_fok(psmq_subscribe(&sub_psmq[i], sub_topics[i]));
		mt_fok(psmqt_receive_expect(&sub_psmq[i], 's', 0, 0,
					sub_topics[i], NULL));
	}

	for (i = 0; i != matches_num; ++i)
	{
		int             j;
		unsigned char   match;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


		mt_fok(psmq_publish(&pub_psmq, matches[i].topic, NULL, 0));

		/* receive messages on expected client and
		 * check if received message is really what was expected */
		for (j = 0; (match = matches[i].matches[j]) != UCHAR_MAX ;++j)
			mt_fok(psmqt_receive_expect(&sub_psmq[match], 'p', 0, 0,
						matches[i].topic, NULL));
	}

	for (i = 0; i != sub_topics_num; ++i)
	{
		struct psmq_msg  msg;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


		/* make non blocking read on all subscribe clients, they
		 * should all fail, as we read all messages in previous
		 * loop. If any of the client no returns message, that
		 * means broker has sent message to the client that was no
		 * suppose to receive it.  */

#if __QNX__ || __QNXNTO
		/* qnx (up to 6.4.0 anyway) has a bug, which causes mq_timedreceive
		 * to return EINTR instead of ETIMEDOUT when timeout occurs
		 *
		 * Don't ask me why, but in this case qnx will return ETIMEDOUT.
		 * I am not even trying to guess what the fuck is going on in
		 * this system */
		mt_ferr(psmq_timedreceive_ms(&sub_psmq[i], &msg, 0), EINTR);
#else
		mt_ferr(psmq_timedreceive_ms(&sub_psmq[i], &msg, 0), ETIMEDOUT);
#endif

		/* by the way, cleanup clients as we are done */
		mt_fok(psmq_cleanup(&sub_psmq[i]));
		mq_unlink(sub_qname[i]);
	}

	mt_fok(psmq_cleanup(&pub_psmq));
	mq_unlink(pub_qname);
	free(sub_psmq);
	free(sub_qname);
}


/* ==========================================================================
                           __               __
                          / /_ ___   _____ / /_ _____
                         / __// _ \ / ___// __// ___/
                        / /_ /  __/(__  )/ /_ (__  )
                        \__/ \___//____/ \__//____/

   ========================================================================== */


static void psmqd_start_stop(void)
{
	/* it's all done in prepare and cleanup test */
	return;
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_create_client(void)
{
	char         qname[QNAME_LEN];
	struct psmq  psmq;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_queue_name(qname, sizeof(qname));
	mt_assert(psmq_init_named(&psmq, gt_broker_name, qname, 10) == 0);
	mt_fok(psmq_cleanup(&psmq));
	mq_unlink(qname);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_create_multiple_client(void)
{
	char         qname[2][QNAME_LEN];
	struct psmq  psmq[2];
	int          i;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_unique_queue_name_array(qname, 2, QNAME_LEN);

	for (i = 0; i != 2; ++i)
		mt_assert(psmq_init_named(&psmq[i], gt_broker_name, qname[i], 10) == 0);

	for (i = 0; i != 2; ++i)
	{
		mt_fok(psmq_cleanup(&psmq[i]));
		mq_unlink(qname[i]);
	}
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_create_max_client(void)
{
	char         qname[PSMQ_MAX_CLIENTS][QNAME_LEN];
	struct psmq  psmq[PSMQ_MAX_CLIENTS];
	int          i;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_unique_queue_name_array(qname, PSMQ_MAX_CLIENTS, QNAME_LEN);

	for (i = 0; i != PSMQ_MAX_CLIENTS; ++i)
		mt_fail(psmq_init_named(&psmq[i], gt_broker_name, qname[i], 10) == 0);

	for (i = 0; i != PSMQ_MAX_CLIENTS; ++i)
	{
		mt_fok(psmq_cleanup(&psmq[i]));
		mq_unlink(qname[i]);
	}
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_create_too_much_client(void)
{
	char         qname[PSMQ_MAX_CLIENTS + 2][QNAME_LEN];
	struct psmq  psmq[PSMQ_MAX_CLIENTS + 2];
	int          i;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	psmqt_gen_unique_queue_name_array(qname, PSMQ_MAX_CLIENTS + 2, QNAME_LEN);

	for (i = 0; i != PSMQ_MAX_CLIENTS; ++i)
		mt_fail(psmq_init_named(&psmq[i], gt_broker_name, qname[i], 10) == 0);

	/* now create 2 clients that won't fit into broker memory */
	mt_ferr(psmq_init_named(&psmq[i], gt_broker_name, qname[i], 10), ENOSPC);
	++i;
	mt_ferr(psmq_init_named(&psmq[i], gt_broker_name, qname[i], 10), ENOSPC);

	for (i = 0; i != PSMQ_MAX_CLIENTS; ++i)
	{
		mt_fok(psmq_cleanup(&psmq[i]));
		mq_unlink(qname[i]);
	}

	/* now that slots are free, again try to connect those
	 * two failed connections */
	mt_fail(psmq_init_named(&psmq[i], gt_broker_name, qname[i], 10) == 0);
	++i;
	mt_fail(psmq_init_named(&psmq[i], gt_broker_name, qname[i], 10) == 0);

	--i;
	mt_fok(psmq_cleanup(&psmq[i]));
	mq_unlink(qname[i]);
	++i;
	mt_fok(psmq_cleanup(&psmq[i]));
	mq_unlink(qname[i]);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_send_empty_msg(void)
{
	mt_fok(psmq_publish(&gt_pub_psmq, "/t", NULL, 0));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'p', 0, 0, "/t", NULL));
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_send_msg(void)
{
	mt_fok(psmq_publish(&gt_pub_psmq, "/t", "t", 2));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'p', 0, 2, "/t", "t"));
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_send_full_msg(void)
{
	char             buf[PSMQ_MSG_MAX - 3];
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_random_string(buf, sizeof(buf));
	buf[0] = '\0';

	mt_fok(psmq_publish(&gt_pub_psmq, "/t", buf, sizeof(buf)));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'p', 0, sizeof(buf), "/t", buf));
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_send_too_big_msg(void)
{
	struct psmq_msg  msg;
	struct psmq_msg  msge;
	struct psmq_msg  pub;
	char             buf[PSMQ_MSG_MAX + 10];
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_random_string(buf, sizeof(buf));
	memset(&msg, 0x00, sizeof(msg));
	memset(&msge, 0x00, sizeof(msge));
	memset(&pub, 0x00, sizeof(pub));

	mt_ferr(psmq_publish(&gt_pub_psmq, "/t", buf, sizeof(buf)), ENOBUFS);

	pub.data[0] = '/';
	pub.data[1] = 't';
	pub.paylen = PSMQ_MSG_MAX + 20;
	mq_send(gt_pub_psmq.qpub, (const char *)&pub, sizeof(pub), 0);

#if __QNX__ || __QNXNTO
	/* qnx (up to 6.4.0 anyway) has a bug, which causes mq_timedreceive
	 * to return EINTR instead of ETIMEDOUT when timeout occurs */
	mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, &msg, 100), EINTR);
#else
	mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, &msg, 100), ETIMEDOUT);
#endif
	mt_fail(memcmp(&msg, &msge, sizeof(msg)) == 0);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_send_unknown_control_msg(void)
{
	struct psmq_msg  msg;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	msg.ctrl.cmd = 'x';
	msg.ctrl.data = gt_pub_psmq.fd;
	msg.paylen = 0;
	msg.data[0] = 0;
	mt_fok(psmq_publish_msg(&gt_pub_psmq, 'x', 0, NULL, NULL, 0, 0));
#if __QNX__ || __QNXNTO
	/* qnx (up to 6.4.0 anyway) has a bug, which causes mq_timedreceive
	 * to return EINTR instead of ETIMEDOUT when timeout occurs */
	mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, &msg, 100), EINTR);
#else
	mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, &msg, 100), ETIMEDOUT);
#endif
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_send_msg_when_noone_is_listening(void)
{
	char             buf[PSMQ_MSG_MAX - 3];
	struct psmq_msg  msg;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_random_string(buf, sizeof(buf));
	buf[0] = '\0';

	mt_fok(psmq_publish(&gt_pub_psmq, "/x", buf, sizeof(buf)));
#if __QNX__ || __QNXNTO
	/* qnx (up to 6.4.0 anyway) has a bug, which causes mq_timedreceive
	 * to return EINTR instead of ETIMEDOUT when timeout occurs
	 */
	mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, &msg, 100), EINTR);
#else
	mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, &msg, 100), ETIMEDOUT);
#endif
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_subscribe_with_bad_topics(void)
{
	int              i;
	char             qname[QNAME_LEN];
	struct psmq      psmq;
	struct psmq_msg  msg;
	const char      *topics[] = { "", "/", "a", "a/", "//", "-d"
#if PSMQ_MSG_MAX > 3
		, "/a/"
#endif

#if PSMQ_MSG_MAX > 4
		, "/a//"
#endif

#if PSMQ_MSG_MAX > 5
		, "/a//a"
#endif
	};
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_queue_name(qname, sizeof(qname));
	mt_assert(psmq_init_named(&psmq, gt_broker_name, qname, 10) == 0);

	memset(&msg, 0x00, sizeof(msg));

	for (i = 0; i != sizeof(topics)/sizeof(*topics); ++i)
	{
		mt_fok(psmq_publish_msg(&psmq, 's', psmq.fd, topics[i], NULL, 0, 0));
		mt_fok(psmqt_receive_expect(&psmq, 's', (unsigned char)EBADMSG,
					0, topics[i], NULL));
	}

	mt_fok(psmq_cleanup(&psmq));
	mq_unlink(qname);

}


/* ==========================================================================
   ========================================================================== */


static void psmqd_unsubscribe(void)
{
	char             buf[PSMQ_MSG_MAX - 3];
	struct psmq_msg  msg;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_random_string(buf, sizeof(buf));

	mt_fok(psmq_publish(&gt_pub_psmq, "/t", buf, sizeof(buf)));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'p', 0, sizeof(buf), "/t", buf));

	mt_fok(psmq_unsubscribe(&gt_sub_psmq, "/t"));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'u', 0, 0, "/t", NULL));

	mt_fok(psmq_publish(&gt_pub_psmq, "/t", buf, sizeof(buf)));
#if __QNX__ || __QNXNTO
	/* qnx (up to 6.4.0 anyway) has a bug, which causes mq_timedreceive
	 * to return EINTR instead of ETIMEDOUT when timeout occurs */
	mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, &msg, 100), EINTR);
#else
	mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, &msg, 100), ETIMEDOUT);
#endif
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_unsubscribe_with_bad_topics(void)
{
	char             qname[QNAME_LEN];
	struct psmq      psmq;
	struct psmq_msg  msg;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_queue_name(qname, sizeof(qname));
	mt_assert(psmq_init_named(&psmq, gt_broker_name, qname, 10) == 0);
	mt_fok(psmq_subscribe(&psmq, "/t"));
	mt_fok(psmqt_receive_expect(&psmq, 's', 0, 0, "/t", NULL));

	memset(&msg, 0x00, sizeof(msg));

	mt_ferr(psmq_unsubscribe(&psmq, "a"), EBADMSG);
	mt_ferr(psmq_unsubscribe(&psmq, NULL), EINVAL);
	mt_ferr(psmq_unsubscribe(&psmq, ""), EINVAL);
	mt_fok(psmq_unsubscribe(&psmq, "/a/"));
	mt_fok(psmqt_receive_expect(&psmq, 'u', ENOENT, 0, "/a/", NULL));

	mt_fok(psmq_cleanup(&psmq));
	mq_unlink(qname);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_send_msg_with_bad_fd(void)
{
	char             qname[QNAME_LEN];
	struct psmq      psmq;
	struct psmq_msg  msg;
	struct psmq_msg  msge;
	const char      *topics = "csued";
	char             t;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_queue_name(qname, sizeof(qname));
	mt_assert(psmq_init_named(&psmq, gt_broker_name, qname, 10) == 0);


	memset(&msg, 0x00, sizeof(msg));
	memset(&msge, 0x00, sizeof(msge));

	while ((t = *topics++))
	{
		mt_fok(psmq_publish_msg(&psmq, t, PSMQ_MAX_CLIENTS, NULL, NULL, 0, 0));
		mt_fok(psmq_publish_msg(&psmq, t, PSMQ_MAX_CLIENTS+1, NULL, NULL, 0, 0));
		mt_fok(psmq_publish_msg(&psmq, t, UCHAR_MAX, NULL, NULL, 0, 0));
	}

#if __QNX__ || __QNXNTO
	/* qnx (up to 6.4.0 anyway) has a bug, which causes mq_timedreceive
	 * to return EINTR instead of ETIMEDOUT when timeout occurs */
	mt_ferr(psmq_timedreceive_ms(&psmq, &msg, 100), EINTR);
#else
	mt_ferr(psmq_timedreceive_ms(&psmq, &msg, 100), ETIMEDOUT);
#endif
	mt_fail(memcmp(&msg, &msge, sizeof(msg)) == 0);
	mt_fok(psmq_cleanup(&psmq));
	mq_unlink(qname);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_detect_dead_client(void)
{
	char             qname[2][QNAME_LEN];
	struct psmq      pub_psmq;
	struct psmq      sub_psmq;
	struct psmq_msg  msg;
	struct timespec  tp;
	char             i;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_unique_queue_name_array(qname, 2, QNAME_LEN);
	mt_fail(psmq_init_named(&pub_psmq, gt_broker_name, qname[0], 10) == 0);
	mt_fail(psmq_init_named(&sub_psmq, gt_broker_name, qname[1], 10) == 0);
	mt_fok(psmq_subscribe(&sub_psmq, "/t"));
	mt_fok(psmqt_receive_expect(&sub_psmq, 's', 0, 0, "/t", NULL));

	for (i = 0; i != 10; ++i)
		mt_fok(psmq_publish(&pub_psmq, "/t", &i, sizeof(i)));
	/* from now on, messages will be lost and missed pubs should
	 * be increased in broker */
	for (i = 0; i != 10; ++i)
		mt_fok(psmq_publish(&pub_psmq, "/t", &i, sizeof(i)));

	/* need to give broker some time to actually publish messages
	 * before we read them, and making more space on the queue */
#ifdef HIGH_LOAD_ENV
	tp.tv_sec = 20;
#else
	tp.tv_sec = 1;
#endif
	tp.tv_nsec = 0;
	nanosleep(&tp, NULL);

	/* last message should have caused broker to remove sub_psmq
	 * client, remove oldest published message and put close
	 * message on queue, let's check if that's true */
	for (i = 1; i != 10; ++i)
		mt_fok(psmqt_receive_expect(&sub_psmq, 'p', 0, sizeof(i), "/t", &i));

	mt_fok(psmqt_receive_expect(&sub_psmq, 'c', 0, 0, NULL, NULL));

	mt_fok(psmq_cleanup(&pub_psmq));
	mt_fok(psmq_cleanup(&sub_psmq));
	mq_unlink(qname[0]);
	mq_unlink(qname[1]);
}


/* ==========================================================================
   ========================================================================== */

#if PSMQ_MSG_MAX > 5

static void psmqd_multi_pub_sub(void *arg)
{
	struct multi_ps_targs  *pub_targs;
	struct multi_ps_targs  *sub_targs;
	struct multi_ps        *args;
	struct psmq            *psmq_sub;
	struct psmq            *psmq_pub;
	struct psmqd_tl       **sub_tl;
	struct psmqd_tl       **pub_tl;
	pthread_t              *sub_t;
	pthread_t              *pub_t;
	char                  **qpub;
	char                  **qsub;
	int                     i;
	int                     j;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	args = arg;

	/* create all clients */
	psmq_pub = calloc(args->num_pub, sizeof(*psmq_pub));
	qpub = calloc(args->num_pub, sizeof(char *));
	for (i = 0; i != args->num_pub; ++i)
	{
		qpub[i] = (char *)malloc(sizeof(char) * QNAME_LEN);
		psmqt_gen_queue_name(qpub[i], QNAME_LEN);
		mt_fok(psmq_init_named(&psmq_pub[i], gt_broker_name, qpub[i], 10));
	}

	psmq_sub = calloc(args->num_sub, sizeof(*psmq_sub));
	qsub = calloc(args->num_sub, sizeof(char *));
	for (i = 0; i != args->num_sub; ++i)
	{
		qsub[i] = malloc(sizeof(char) * QNAME_LEN);
		psmqt_gen_queue_name(qsub[i], QNAME_LEN);
		mt_fok(psmq_init_named(&psmq_sub[i], gt_broker_name, qsub[i], 10));
		/* subscribe to "/s" topic, this is "stop" message to
		 * thread so it knows when to exit */
		mt_fok(psmq_subscribe(&psmq_sub[i], "/s"));
		mt_fok(psmqt_receive_expect(&psmq_sub[i], 's', 0, 0, "/s", NULL));
	}

	/* clients created, now subscribe all sub clients
	 * algorithm is to subscribe to topic each time binary
	 * 1 is in number, so subscribe client with... ah whatever
	 * table will show it better than words
	 *
	 *  id (i) | in binary | topics subscribed
	 * --------+-----------+-------------------
	 *     0   |  0b00000  | [none]
	 *     1   |  0b00001  | "/1"
	 *     2   |  0b00010  |      "/2"
	 *     3   |  0b00011  | "/1" "/2"
	 *     4   |  0b00100  |           "/3"
	 *     5   |  0b00101  | "/1"      "/3"
	 *     6   |  0b00110  |      "/2" "/3"
	 *     7   |  0b00111  | "/1" "/2" "/3"
	 *
	 * yup, that's clear now */

	for (i = 0; i != args->num_sub; ++i)
	{
		char topic[2 + 1];
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		topic[0] = '/';
		topic[2] = '\0';

		for (j = 0; j != 9; ++j)
		{
			if (i & 1 << j)
			{
				topic[1] = '1' + j;
				mt_fok(psmq_subscribe(&psmq_sub[i], topic));
				mt_fok(psmqt_receive_expect(&psmq_sub[i],
							's', 0, 0, topic, NULL));
				mt_fok(psmq_ioctl_reply_timeout(&psmq_sub[i], 100));
				/* receive anything to pop reply from the queue, we
				 * don't need to verify it as this route is verified
				 * elsewhere */
				psmqt_receive_expect(&psmq_sub[i], 'i', 0, 0, NULL, NULL);
			}
		}
	}

	/* now create threads that will publish and receive data in
	 * parallel, each thread gets a list that will be populated
	 * with data sent/received, later we will be checking it if it
	 * contains all expected data
	 *
	 *
	 * note: that list is a little bit hacked. topic list was
	 * designed to hold only topics - but will work for with any
	 * string, so to store both data and topic, data is
	 * alphanumeric and format is
	 *
	 * "TOPIC DDDDDDD"
	 *
	 * That is, first bytes are for topic, then one space character
	 * (as delimiter between topic and data) and then data till the
	 * end of string */

	sub_t = calloc(args->num_sub, sizeof(*sub_t));
	sub_tl = calloc(args->num_sub, sizeof(*pub_tl));
	sub_targs = calloc(args->num_sub, sizeof(*sub_targs));
	for (i = 0; i != args->num_sub; ++i)
	{
		sub_targs[i].tl = &sub_tl[i];
		sub_targs[i].psmq = &psmq_sub[i];
		pthread_create(&sub_t[i], NULL, multi_sub_thread, &sub_targs[i]);
	}

	pub_t = calloc(args->num_pub, sizeof(*pub_t));
	pub_tl = calloc(args->num_pub, sizeof(*pub_tl));
	pub_targs = calloc(args->num_pub, sizeof(*sub_targs));
	for (i = 0; i != args->num_pub; ++i)
	{
		pub_targs[i].id = i;
		pub_targs[i].num_msg = 128;
		pub_targs[i].tl = &pub_tl[i];
		pub_targs[i].psmq = &psmq_pub[i];
		pthread_create(&pub_t[i], NULL, multi_pub_thread, &pub_targs[i]);
	}

	/* threads started, wait for publishers to
	 * finish their jobs */
	for (i = 0; i != args->num_pub; ++i)
		pthread_join(pub_t[i], NULL);

	/* tell subscribers to stop receiving and exit,
	 * all sub threads are subscribe to /s so it's
	 * enough to send stop by one client only, and
	 * all clients will receive it */
	psmq_publish(&psmq_pub[0], "/s", NULL, 0);

	/* wait for all subscribers to finish
	 * receiving data and exit */
	for (i = 0; i != args->num_sub; ++i)
		pthread_join(sub_t[i], NULL);

	/* ok, publishers published, subscribers received, but
	 * was it successfull? Let's find out. Check that
	 * sbuscribers have all messages (and nothing more)
	 * that publishers sent out */
	for (i = 0; i != args->num_pub; ++i)
	{
		/* check if messages from this publish client was
		 * received by proper clients.  */
		for (j = 0; j != args->num_sub; ++j)
		{
			struct psmqd_tl  *node;
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


			/* this sub client should not receive messages from
			 * this pub client */
			if (!(j & 1 << i))
				continue;

			/* iterate through all messages send by current pub,
			 * and remove that message from subsribed message list.
			 * If psmqd_tl_delete() returns -1, it means sub client
			 * did not receive message, and that is wrong */
			for (node = pub_tl[i]; node != NULL; node = node->next)
				mt_fok(psmqd_tl_delete(&sub_tl[j], node->topic));
		}
	}

	/* we should have deleted all elements in the sub clients'
	 * list, if not, that means either publish function fucked and
	 * didn't created list node with published message (less
	 * likely) or broker did send message to the client which did
	 * not subscribe for it (more likely) */

	for (i = 0; i != args->num_sub; ++i)
		mt_fail(sub_tl[i] == NULL);

	/* all test done, time to clean this shit up */
	for (i = 0; i != args->num_sub; ++i)
	{
		mt_fok(psmq_cleanup(&psmq_sub[i]));
		psmqt_receive_expect(&psmq_sub[i], 'c', 0, 0, NULL, NULL);
		mq_unlink(qsub[i]);
		free(qsub[i]);
	}
	free(sub_t);
	free(sub_tl);
	free(sub_targs);
	free(qsub);
	free(psmq_sub);

	for (i = 0; i != args->num_pub; ++i)
	{
		mt_fok(psmq_cleanup(&psmq_pub[i]));
		psmqt_receive_expect(&psmq_pub[i], 'c', 0, 0, NULL, NULL);
		mq_unlink(qpub[i]);
		psmqd_tl_destroy(pub_tl[i]);
		free(qpub[i]);
	}
	free(pub_t);
	free(pub_tl);
	free(pub_targs);
	free(qpub);
	free(psmq_pub);
}

#endif

/* ==========================================================================
   ========================================================================== */


static void psmqd_topic_plus_wildcard(void)
{
	const char *sub_topics[] =
	{
		"/+"       /*  0 */

#if PSMQ_MSG_MAX >= 5 && PSMQ_MAX_CLIENTS >= 5
			,
		"/a/+",    /*  1 */
		"/+/b",    /*  2 */
		"/+/+"     /*  3 */
#endif

#if PSMQ_MSG_MAX >= 7 && PSMQ_MAX_CLIENTS >= 12
			,
		"/a/b/+",  /*  4 */
		"/a/+/c",  /*  5 */
		"/a/+/+",  /*  6 */
		"/+/b/c",  /*  7 */
		"/+/b/+",  /*  8 */
		"/+/+/c",  /*  9 */
		"/+/+/+"   /* 10 */
#endif
	};

	struct wildcard_matches msgs[] =
	{
		{ "/a",     { 0, UCHAR_MAX } },
		{ "/b",     { 0, UCHAR_MAX } },
		{ "/c",     { 0, UCHAR_MAX } }

#if PSMQ_MSG_MAX >= 5 && PSMQ_MAX_CLIENTS >= 5
		,
		{ "/aab",   { 0, UCHAR_MAX } },
		{ "/a/a",   { 1, 3, UCHAR_MAX } },
		{ "/a/b",   { 1, 2, 3, UCHAR_MAX } },
		{ "/a/c",   { 1, 3, UCHAR_MAX } },
		{ "/b/a",   { 3, UCHAR_MAX } },
		{ "/b/b",   { 2, 3, UCHAR_MAX } },
		{ "/b/c",   { 3, UCHAR_MAX } },
		{ "/c/a",   { 3, UCHAR_MAX } },
		{ "/c/b",   { 2, 3, UCHAR_MAX } },
		{ "/c/c",   { 3, UCHAR_MAX } }
#endif

#if PSMQ_MSG_MAX >= 7 && PSMQ_MAX_CLIENTS >= 12
		,
		{ "/a/bbb", { 1, 3, UCHAR_MAX } },
		{ "/aa/b",  { 2, 3, UCHAR_MAX } },
		{ "/aa/bb", { 3, UCHAR_MAX } },
		{ "/a/a/a", { 6, 10, UCHAR_MAX } },
		{ "/a/a/b", { 6, 10, UCHAR_MAX } },
		{ "/a/a/c", { 5, 6, 9, 10, UCHAR_MAX } },
		{ "/a/b/a", { 4, 6, 8, 10, UCHAR_MAX } },
		{ "/a/b/b", { 4, 6, 8, 10, UCHAR_MAX } },
		{ "/a/b/c", { 4, 5, 6, 7, 8, 9, 10, UCHAR_MAX } },
		{ "/a/c/a", { 6, 10, UCHAR_MAX } },
		{ "/a/c/b", { 6, 10, UCHAR_MAX } },
		{ "/a/c/c", { 5, 6, 9, 10, UCHAR_MAX } },
		{ "/b/a/a", { 10, UCHAR_MAX } },
		{ "/b/a/b", { 10, UCHAR_MAX } },
		{ "/b/a/c", { 9, 10, UCHAR_MAX } },
		{ "/b/b/a", { 8, 10, UCHAR_MAX } },
		{ "/b/b/b", { 8, 10, UCHAR_MAX } },
		{ "/b/b/c", { 7, 8, 9, 10, UCHAR_MAX } },
		{ "/b/c/a", { 10, UCHAR_MAX } },
		{ "/b/c/b", { 10, UCHAR_MAX } },
		{ "/b/c/c", { 9, 10, UCHAR_MAX } },
		{ "/c/a/a", { 10, UCHAR_MAX } },
		{ "/c/a/b", { 10, UCHAR_MAX } },
		{ "/c/a/c", { 9, 10, UCHAR_MAX } },
		{ "/c/b/a", { 8, 10, UCHAR_MAX } },
		{ "/c/b/b", { 8, 10, UCHAR_MAX } },
		{ "/c/b/c", { 7, 8, 9, 10, UCHAR_MAX } },
		{ "/c/c/a", { 10, UCHAR_MAX } },
		{ "/c/c/b", { 10, UCHAR_MAX } },
		{ "/c/c/c", { 9, 10, UCHAR_MAX } }
#endif

	};
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	psmqd_wildard_test(sub_topics, sizeof(sub_topics)/sizeof(*sub_topics),
			msgs, sizeof(msgs)/sizeof(*msgs));
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_topic_star_wildcard(void)
{
	const char *sub_topics[] =
	{
		"/*"       /*  0 */

#if PSMQ_MSG_MAX >= 5 && PSMQ_MAX_CLIENTS >= 3
		,
		"/a/*"     /*  1 */
#endif

#if PSMQ_MSG_MAX >= 7 && PSMQ_MAX_CLIENTS >= 4
		,
		"/a/b/*"   /*  2 */

#endif
	};

	struct wildcard_matches msgs[] =
	{
		{ "/a",     { 0, UCHAR_MAX } },
		{ "/b",     { 0, UCHAR_MAX } },
		{ "/c",     { 0, UCHAR_MAX } }

#if PSMQ_MSG_MAX >= 5 && PSMQ_MAX_CLIENTS >= 3
		,
		{ "/aab",   { 0, UCHAR_MAX } },
		{ "/a/a",   { 0, 1, UCHAR_MAX } },
		{ "/a/b",   { 0, 1, UCHAR_MAX } },
		{ "/a/c",   { 0, 1, UCHAR_MAX } },
		{ "/b/a",   { 0, UCHAR_MAX } },
		{ "/b/b",   { 0, UCHAR_MAX } },
		{ "/b/c",   { 0, UCHAR_MAX } },
		{ "/c/a",   { 0, UCHAR_MAX } },
		{ "/c/b",   { 0, UCHAR_MAX } },
		{ "/c/c",   { 0, UCHAR_MAX } }
#endif

#if PSMQ_MSG_MAX >= 7 && PSMQ_MAX_CLIENTS >= 4
		,
		{ "/a/bbb", { 0, 1, UCHAR_MAX } },
		{ "/aa/b",  { 0, UCHAR_MAX } },
		{ "/aa/bb", { 0, UCHAR_MAX } },
		{ "/a/a/a", { 0, 1, UCHAR_MAX } },
		{ "/a/a/b", { 0, 1, UCHAR_MAX } },
		{ "/a/a/c", { 0, 1, UCHAR_MAX } },
		{ "/a/b/a", { 0, 1, 2, UCHAR_MAX } },
		{ "/a/b/b", { 0, 1, 2, UCHAR_MAX } },
		{ "/a/b/c", { 0, 1, 2, UCHAR_MAX } },
		{ "/a/c/a", { 0, 1, UCHAR_MAX } },
		{ "/a/c/b", { 0, 1, UCHAR_MAX } },
		{ "/a/c/c", { 0, 1, UCHAR_MAX } },
		{ "/b/a/a", { 0, UCHAR_MAX } },
		{ "/b/a/b", { 0, UCHAR_MAX } },
		{ "/b/a/c", { 0, UCHAR_MAX } },
		{ "/b/b/a", { 0, UCHAR_MAX } },
		{ "/b/b/b", { 0, UCHAR_MAX } },
		{ "/b/b/c", { 0, UCHAR_MAX } },
		{ "/b/c/a", { 0, UCHAR_MAX } },
		{ "/b/c/b", { 0, UCHAR_MAX } },
		{ "/b/c/c", { 0, UCHAR_MAX } },
		{ "/c/a/a", { 0, UCHAR_MAX } },
		{ "/c/a/b", { 0, UCHAR_MAX } },
		{ "/c/a/c", { 0, UCHAR_MAX } },
		{ "/c/b/a", { 0, UCHAR_MAX } },
		{ "/c/b/b", { 0, UCHAR_MAX } },
		{ "/c/b/c", { 0, UCHAR_MAX } },
		{ "/c/c/a", { 0, UCHAR_MAX } },
		{ "/c/c/b", { 0, UCHAR_MAX } },
		{ "/c/c/c", { 0, UCHAR_MAX } }
#endif

	};
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	psmqd_wildard_test(sub_topics, sizeof(sub_topics)/sizeof(*sub_topics),
			msgs, sizeof(msgs)/sizeof(*msgs));
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_topic_mixed_wildcard(void)
{
	const char *sub_topics[] =
	{
#if PSMQ_MSG_MAX >= 5 && PSMQ_MAX_CLIENTS >= 2
		"/+/*"     /* 0 */
#endif

#if PSMQ_MSG_MAX >= 7 && PSMQ_MAX_CLIENTS >= 5
		,
		"/a/+/*",  /* 1 */
		"/+/b/*",  /* 2 */
		"/+/+/*"   /* 3 */
#endif
	};

	struct wildcard_matches msgs[] =
	{
		{ "/a",     { UCHAR_MAX } },
		{ "/b",     { UCHAR_MAX } },
		{ "/c",     { UCHAR_MAX } }

#if PSMQ_MSG_MAX >= 5 && PSMQ_MAX_CLIENTS >= 2
		,
		{ "/aab",   { UCHAR_MAX } },
		{ "/a/a",   { 0, UCHAR_MAX } },
		{ "/a/b",   { 0, UCHAR_MAX } },
		{ "/a/c",   { 0, UCHAR_MAX } },
		{ "/b/a",   { 0, UCHAR_MAX } },
		{ "/b/b",   { 0, UCHAR_MAX } },
		{ "/b/c",   { 0, UCHAR_MAX } },
		{ "/c/a",   { 0, UCHAR_MAX } },
		{ "/c/b",   { 0, UCHAR_MAX } },
		{ "/c/c",   { 0, UCHAR_MAX } }
#endif

#if PSMQ_MSG_MAX >= 7 && PSMQ_MAX_CLIENTS >= 5
		,
		{ "/a/bbb", { 0, UCHAR_MAX } },
		{ "/aa/b",  { 0, UCHAR_MAX } },
		{ "/aa/bb", { 0, UCHAR_MAX } },
		{ "/a/a/a", { 0, 1, 3, UCHAR_MAX } },
		{ "/a/a/b", { 0, 1, 3, UCHAR_MAX } },
		{ "/a/a/c", { 0, 1, 3, UCHAR_MAX } },
		{ "/a/b/a", { 0, 1, 2, 3, UCHAR_MAX } },
		{ "/a/b/b", { 0, 1, 2, 3, UCHAR_MAX } },
		{ "/a/b/c", { 0, 1, 2, 3, UCHAR_MAX } },
		{ "/a/c/a", { 0, 1, 3, UCHAR_MAX } },
		{ "/a/c/b", { 0, 1, 3, UCHAR_MAX } },
		{ "/a/c/c", { 0, 1, 3, UCHAR_MAX } },
		{ "/b/a/a", { 0, 3, UCHAR_MAX } },
		{ "/b/a/b", { 0, 3, UCHAR_MAX } },
		{ "/b/a/c", { 0, 3, UCHAR_MAX } },
		{ "/b/b/a", { 0, 2, 3, UCHAR_MAX } },
		{ "/b/b/b", { 0, 2, 3, UCHAR_MAX } },
		{ "/b/b/c", { 0, 2, 3, UCHAR_MAX } },
		{ "/b/c/a", { 0, 3, UCHAR_MAX } },
		{ "/b/c/b", { 0, 3, UCHAR_MAX } },
		{ "/b/c/c", { 0, 3, UCHAR_MAX } },
		{ "/c/a/a", { 0, 3, UCHAR_MAX } },
		{ "/c/a/b", { 0, 3, UCHAR_MAX } },
		{ "/c/a/c", { 0, 3, UCHAR_MAX } },
		{ "/c/b/a", { 0, 2, 3, UCHAR_MAX } },
		{ "/c/b/b", { 0, 2, 3, UCHAR_MAX } },
		{ "/c/b/c", { 0, 2, 3, UCHAR_MAX } },
		{ "/c/c/a", { 0, 3, UCHAR_MAX } },
		{ "/c/c/b", { 0, 3, UCHAR_MAX } },
		{ "/c/c/c", { 0, 3, UCHAR_MAX } }
#endif

#if PSMQ_MSG_MAX >= 9 && PSMQ_MAX_CLIENTS >= 5
		,
		{ "/a/bbb",   { 0, UCHAR_MAX } },
		{ "/aa/b",    { 0, UCHAR_MAX } },
		{ "/aa/bb",   { 0, UCHAR_MAX } },
		{ "/a/a/a/d", { 0, 1, 3, UCHAR_MAX } },
		{ "/a/a/b/d", { 0, 1, 3, UCHAR_MAX } },
		{ "/a/a/c/d", { 0, 1, 3, UCHAR_MAX } },
		{ "/a/b/a/d", { 0, 1, 2, 3, UCHAR_MAX } },
		{ "/a/b/b/d", { 0, 1, 2, 3, UCHAR_MAX } },
		{ "/a/b/c/d", { 0, 1, 2, 3, UCHAR_MAX } },
		{ "/a/c/a/d", { 0, 1, 3, UCHAR_MAX } },
		{ "/a/c/b/d", { 0, 1, 3, UCHAR_MAX } },
		{ "/a/c/c/d", { 0, 1, 3, UCHAR_MAX } },
		{ "/b/a/a/d", { 0, 3, UCHAR_MAX } },
		{ "/b/a/b/d", { 0, 3, UCHAR_MAX } },
		{ "/b/a/c/d", { 0, 3, UCHAR_MAX } },
		{ "/b/b/a/d", { 0, 2, 3, UCHAR_MAX } },
		{ "/b/b/b/d", { 0, 2, 3, UCHAR_MAX } },
		{ "/b/b/c/d", { 0, 2, 3, UCHAR_MAX } },
		{ "/b/c/a/d", { 0, 3, UCHAR_MAX } },
		{ "/b/c/b/d", { 0, 3, UCHAR_MAX } },
		{ "/b/c/c/d", { 0, 3, UCHAR_MAX } },
		{ "/c/a/a/d", { 0, 3, UCHAR_MAX } },
		{ "/c/a/b/d", { 0, 3, UCHAR_MAX } },
		{ "/c/a/c/d", { 0, 3, UCHAR_MAX } },
		{ "/c/b/a/d", { 0, 2, 3, UCHAR_MAX } },
		{ "/c/b/b/d", { 0, 2, 3, UCHAR_MAX } },
		{ "/c/b/c/d", { 0, 2, 3, UCHAR_MAX } },
		{ "/c/c/a/d", { 0, 3, UCHAR_MAX } },
		{ "/c/c/b/d", { 0, 3, UCHAR_MAX } },
		{ "/c/c/c/d", { 0, 3, UCHAR_MAX } }
#endif
	};
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	psmqd_wildard_test(sub_topics, sizeof(sub_topics)/sizeof(*sub_topics),
			msgs, sizeof(msgs)/sizeof(*msgs));
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_reply_to_full_queue(void)
{
	char             qname[2][QNAME_LEN];
	struct psmq      pub_psmq;
	struct psmq      sub_psmq;
	struct psmq_msg  msg;
	struct timespec  tp;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_unique_queue_name_array(qname, 2, QNAME_LEN);
	mt_fail(psmq_init_named(&pub_psmq, gt_broker_name, qname[0], 10) == 0);
	mt_fail(psmq_init_named(&sub_psmq, gt_broker_name, qname[1], 2) == 0);
	mt_fok(psmq_subscribe(&sub_psmq, "/t"));
	mt_fok(psmqt_receive_expect(&sub_psmq, 's', 0, 0, "/t", NULL));
	mt_fok(psmq_publish(&pub_psmq, "/t", "1", 2));
	mt_fok(psmq_publish(&pub_psmq, "/t", "2", 2));
	mt_fok(psmq_publish(&pub_psmq, "/t", "3", 2));

	/* need to give broker some time to actually publish messages
	 * before we read them, and making more space on the queue */
#ifdef HIGH_LOAD_ENV
	tp.tv_sec = 20;
#else
	tp.tv_sec = 1;
#endif
	tp.tv_nsec = 0;
	nanosleep(&tp, NULL);
	mt_fok(psmq_receive(&sub_psmq, &msg));
	mt_fok(psmq_receive(&sub_psmq, &msg));
#if __QNX__ || __QNXNTO
	/* qnx (up to 6.4.0 anyway) has a bug, which causes mq_timedreceive
	 * to return EINTR instead of ETIMEDOUT when timeout occurs */
	mt_ferr(psmq_timedreceive_ms(&sub_psmq, &msg, 100), EINTR);
#else
	mt_ferr(psmq_timedreceive_ms(&sub_psmq, &msg, 100), ETIMEDOUT);
#endif

	mt_fok(psmq_cleanup(&pub_psmq));
	mt_fok(psmq_cleanup(&sub_psmq));
	mq_unlink(qname[0]);
	mq_unlink(qname[1]);

}


/* ==========================================================================
   ========================================================================== */


void psmqd_invalid_ioctl_request(void)
{
	char  buf[16];
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	memset(buf, 0x00, sizeof(buf));
	buf[0] = PSMQ_IOCTL_INVALID;
	mt_fok(psmq_publish_msg(&gt_sub_psmq, 'i', gt_sub_psmq.fd,
				NULL, buf, 1, 0));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'i', EINVAL, 1, NULL, buf));
}


/* ==========================================================================
   ========================================================================== */


void psmqd_invalid_ioctl_request2(void)
{
	char  buf[16];
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	memset(buf, 0x00, sizeof(buf));
	buf[0] = PSMQ_IOCTL_MAX;
	mt_fok(psmq_publish_msg(&gt_sub_psmq, 'i', gt_sub_psmq.fd,
				NULL, buf, 1, 0));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'i', EINVAL, 1, NULL, buf));
}


/* ==========================================================================
   ========================================================================== */


void psmqd_no_ioctl_request(void)
{
	char  buf[16];
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	buf[0] = PSMQ_IOCTL_INVALID;
	mt_fok(psmq_publish_msg(&gt_sub_psmq, 'i', gt_sub_psmq.fd,
				NULL, NULL, 0, 0));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'i', EINVAL, 1, NULL, buf));
}


/* ==========================================================================
             __               __
            / /_ ___   _____ / /_   ____ _ _____ ____   __  __ ____
           / __// _ \ / ___// __/  / __ `// ___// __ \ / / / // __ \
          / /_ /  __/(__  )/ /_   / /_/ // /   / /_/ // /_/ // /_/ /
          \__/ \___//____/ \__/   \__, //_/    \____/ \__,_// .___/
                                 /____/                    /_/
   ========================================================================== */


void psmqd_test_group(void)
{
	int              num_pub_max;
	int              num_sub_max;
	char             mps_name[64];
	struct multi_ps  mps;

#if PSMQ_MAX_CLIENTS < 5
	num_pub_max = 0;
	num_sub_max = 0;
#elif PSMQ_MAX_CLIENTS < 10
	num_pub_max = 2;
	num_sub_max = 3;
#elif PSMQ_MAX_CLIENTS < 19
	num_pub_max = 3;
	num_sub_max = 7;
#elif PSMQ_MAX_CLIENTS < 36
	num_pub_max = 4;
	num_sub_max = 15;
#elif PSMQ_MAX_CLIENTS < 69
	num_pub_max = 5;
	num_sub_max = 31;
#elif PSMQ_MAX_CLIENTS < 134
	num_pub_max = 6;
	num_sub_max = 63;
#else
	num_pub_max = 7;
	num_sub_max = 127;
#endif
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	/* tests that needs broker and use default set of clients for
	 * testing, one subsriber and one publisher */

	mt_prepare_test = psmqt_prepare_test_with_clients;
	mt_cleanup_test = psmqt_cleanup_test_with_clients;

	mt_run(psmqd_send_empty_msg);
	mt_run(psmqd_send_full_msg);
	mt_run(psmqd_send_msg);
	mt_run(psmqd_send_msg_when_noone_is_listening);
	mt_run(psmqd_send_too_big_msg);
	mt_run(psmqd_send_unknown_control_msg);
	mt_run(psmqd_unsubscribe);
	mt_run(psmqd_invalid_ioctl_request);
	mt_run(psmqd_invalid_ioctl_request2);
	mt_run(psmqd_no_ioctl_request);

	/* tests that creates own custom set of clients, and only need
	 * broker to start/stop */

	mt_prepare_test = psmqt_prepare_test;
	mt_cleanup_test = psmqt_cleanup_test;

	mt_run(psmqd_create_client);
	mt_run(psmqd_create_max_client);
	mt_run(psmqd_create_multiple_client);
	mt_run(psmqd_create_too_much_client);
	mt_run(psmqd_send_msg_with_bad_fd);
	mt_run(psmqd_start_stop);
	mt_run(psmqd_subscribe_with_bad_topics);
	mt_run(psmqd_unsubscribe_with_bad_topics);
	mt_run(psmqd_topic_plus_wildcard);
	mt_run(psmqd_topic_star_wildcard);
	mt_run(psmqd_topic_mixed_wildcard);
	mt_run(psmqd_reply_to_full_queue);
	mt_run(psmqd_detect_dead_client);

	for (mps.num_pub = 1; mps.num_pub <= num_pub_max; ++mps.num_pub)
	{
		for (mps.num_sub = 0; mps.num_sub <= num_sub_max; ++mps.num_sub)
		{
			sprintf(mps_name, "[psmqd_multi_pub_sub() num_pub: %d num_sub: %d]",
					mps.num_pub, mps.num_sub);
			mt_run_param_named(psmqd_multi_pub_sub, &mps, mps_name);
		}
	}
}
