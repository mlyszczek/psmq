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


#include <embedlog.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "mtest.h"
#include "psmq.h"
#include "psmqd-startup.h"


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


mt_defs_ext();


/* ==========================================================================
                           __               __
                          / /_ ___   _____ / /_ _____
                         / __// _ \ / ___// __// ___/
                        / /_ /  __/(__  )/ /_ (__  )
                        \__/ \___//____/ \__//____/

   ========================================================================== */


static void psmq_initialize(void)
{
	char         qname[QNAME_LEN];
	struct psmq  psmq;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_queue_name(qname, sizeof(qname));
	mt_fok(psmq_init_named(&psmq, gt_broker_name, qname, 10));
	mt_fok(psmq_cleanup(&psmq));
	mq_unlink(qname);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_initialize_queue_not_exist(void)
{
	struct psmq  psmq;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	mq_unlink("/b");
	mt_ferr(psmq_init_named(&psmq, "/b",  "/q", 10), ENOENT);
	mq_unlink("/q");
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub_max_topic_minus_one(void)
{
	char         topic[PSMQ_MSG_MAX - 1];
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_random_string(topic, sizeof(topic));
	topic[0] = '/';
	mt_fok(psmq_subscribe(&gt_sub_psmq, topic));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 's', 0, 0, topic, NULL));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub_max_topic_plus_one(void)
{
	char         topic[PSMQ_MSG_MAX + 1];
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_random_string(topic, sizeof(topic));
	topic[0] = '/';
	mt_ferr(psmq_subscribe(&gt_pub_psmq, topic), ENOBUFS);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub_max_topic(void)
{
	char         topic[PSMQ_MSG_MAX];
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_random_string(topic, sizeof(topic));
	topic[0] = '/';
	mt_fok(psmq_subscribe(&gt_sub_psmq, topic));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 's', 0, 0, topic, NULL));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_unsub_not_subscribed(void)
{
	mt_fok(psmq_unsubscribe(&gt_sub_psmq, "/x"));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'u', ENOENT, 0, "/x", NULL));
}

/* ==========================================================================
   ========================================================================== */


static void psmq_pub_paylen_and_payload_null(void)
{
	mt_fok(psmq_publish(&gt_pub_psmq, "/t", NULL, 1));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_unsub(void)
{
	mt_fok(psmq_unsubscribe(&gt_sub_psmq, "/t"));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'u', 0, 0, "/t", NULL));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_publish_receive(void)
{
	/* -3 because, topic and data shares single buffer, and 3 bytes
	 * are taken by topic "/t\0" */
	char   buf[PSMQ_MSG_MAX - 3];
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_random_string(buf, sizeof(buf));
	mt_fok(psmq_publish(&gt_pub_psmq, "/t", buf, sizeof(buf)));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'p', 0, sizeof(buf), "/t", buf));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_publish_with_prio(void)
{

	char             buf0[PSMQ_MSG_MAX - 3];
	char             buf1[PSMQ_MSG_MAX - 3];
	char             buf2[PSMQ_MSG_MAX - 3];
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	psmqt_gen_random_string(buf0, sizeof(buf0));
	psmqt_gen_random_string(buf1, sizeof(buf1));
	psmqt_gen_random_string(buf2, sizeof(buf2));

	mt_fok(psmq_publish_prio(&gt_pub_psmq, "/t", buf0, sizeof(buf0), 0));
	mt_fok(psmq_publish_prio(&gt_pub_psmq, "/t", buf1, sizeof(buf1), 3));
	mt_fok(psmq_publish_prio(&gt_pub_psmq, "/t", buf2, sizeof(buf2), 2));

	/* yield and wait a second so all messages
	 * with specific priorities are sent */

	sleep(1);

	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'p', 0, sizeof(buf1), "/t", buf1));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'p', 0, sizeof(buf2), "/t", buf2));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'p', 0, sizeof(buf0), "/t", buf0));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_publish_timedreceive(void)
{
	char             buf[PSMQ_MSG_MAX - 3];
	struct psmq_msg  msg;
	struct timespec  tp;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	memset(&msg, 0x00, sizeof(msg));
	clock_gettime(CLOCK_REALTIME, &tp);
	tp.tv_sec += 1;
	psmqt_gen_random_string(buf, sizeof(buf));
	mt_fok(psmq_publish(&gt_pub_psmq, "/t", buf, sizeof(buf)));
	mt_fok(psmq_timedreceive(&gt_sub_psmq, &msg, &tp));
	tp.tv_sec = 0;
#if __QNX__ || __QNXNTO
	/* qnx (up to 6.4.0 anyway) has a bug, which causes mq_timedreceive
	 * to return EINTR instead of ETIMEDOUT when timeout occurs */
	mt_ferr(psmq_timedreceive(&gt_sub_psmq, &msg, &tp), EINTR);
#else
	mt_ferr(psmq_timedreceive(&gt_sub_psmq, &msg, &tp), ETIMEDOUT);
#endif
	mt_fail(strcmp(msg.data, "/t") == 0);
	mt_fail(memcmp(msg.data + 3, buf, sizeof(buf)) == 0);
	mt_fail(msg.paylen == sizeof(buf));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_publish_timedreceive_ms(void)
{
	char             buf[PSMQ_MSG_MAX - 3];
	struct psmq_msg  msg;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	memset(&msg, 0x00, sizeof(msg));
	psmqt_gen_random_string(buf, sizeof(buf));
	mt_fok(psmq_publish(&gt_pub_psmq, "/t", buf, sizeof(buf)));
	mt_fok(psmq_timedreceive_ms(&gt_sub_psmq, &msg, 999));
#if __QNX__ || __QNXNTO
	/* qnx (up to 6.4.0 anyway) has a bug, which causes mq_timedreceive
	 * to return EINTR instead of ETIMEDOUT when timeout occurs */
	mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, &msg, 100), EINTR);
#else
	mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, &msg, 100), ETIMEDOUT);
#endif
	mt_fail(strcmp(msg.data, "/t") == 0);
	mt_fail(memcmp(msg.data + 3, buf, sizeof(buf)) == 0);
	mt_fail(msg.paylen == sizeof(buf));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub_after_init(void)
{
	mt_fok(psmq_subscribe(&gt_sub_psmq, "/x"));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 's', 0, 0, "/x", NULL));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_unsub_after_init(void)
{
	mt_fok(psmq_unsubscribe(&gt_sub_psmq, "/t"));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'u', 0, 0, "/t", NULL));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_initialize_too_much_clients(void)
{
	char         qname[PSMQ_MAX_CLIENTS + 2][QNAME_LEN];
	struct psmq  psmq[PSMQ_MAX_CLIENTS + 2];
	int          i;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	/* generate unique name for every queue */
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


void psmq_set_reply_timeout(unsigned short timeout)
{
	char            expect[8];
	int             explen;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	timeout = 100;
	expect[0] = PSMQ_IOCTL_REPLY_TIMEOUT;
	memcpy(expect + 1, &timeout, sizeof(timeout));
	explen = 1 + sizeof(timeout);

	mt_fok(psmq_ioctl_reply_timeout(&gt_sub_psmq, timeout));
	mt_fok(psmqt_receive_expect(&gt_sub_psmq, 'i', 0, explen, NULL, expect));
}


/* ==========================================================================
             __               __
            / /_ ___   _____ / /_   ____ _ _____ ____   __  __ ____
           / __// _ \ / ___// __/  / __ `// ___// __ \ / / / // __ \
          / /_ /  __/(__  )/ /_   / /_/ // /   / /_/ // /_/ // /_/ /
          \__/ \___//____/ \__/   \__, //_/    \____/ \__,_// .___/
                                 /____/                    /_/
   ========================================================================== */


void psmq_test_group(void)
{
	struct psmq      psmq_uninit;
	struct psmq      psmq;
	struct psmq_msg  msg;
	char             qname[QNAME_LEN];
	char             buf[PSMQ_MSG_MAX + 10];
	char             long_qname[512];
	struct timespec  tp;
	struct timespec  tp_inval;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	tp_inval.tv_sec = -1;
	tp_inval.tv_nsec = -1;
	memset(&tp, 0x00, sizeof(tp));
	memset(&psmq_uninit, 0x00, sizeof(psmq));
	psmqt_gen_queue_name(qname, sizeof(qname));
	psmq_init_named(&psmq, gt_broker_name, qname, 10);
	psmqt_gen_random_string(buf, sizeof(buf));
	psmqt_gen_random_string(long_qname, sizeof(long_qname));
	long_qname[0] = '/';
	buf[0] = '/';

#define CHECK_ERR(f, e) mt_run_quick(f == -1 && errno == e)
	CHECK_ERR(psmq_init_named(NULL, "/b",  "/q", 10), EINVAL);
	CHECK_ERR(psmq_init_named(&psmq, "b",  "/q", 10), EINVAL);
	CHECK_ERR(psmq_init_named(&psmq, "/b", "q",  10), EINVAL);
	CHECK_ERR(psmq_init_named(&psmq, "",   "/q", 10), EINVAL);
	CHECK_ERR(psmq_init_named(&psmq, "/b", "",   10), EINVAL);
	CHECK_ERR(psmq_init_named(&psmq, "/b", "/q",  0), EINVAL);
	CHECK_ERR(psmq_init_named(&psmq, "/b", "/q", -1), EINVAL);
	CHECK_ERR(psmq_init_named(&psmq, "/b", buf, 10), ENAMETOOLONG);
	CHECK_ERR(psmq_cleanup(NULL), EINVAL);
	CHECK_ERR(psmq_cleanup(&psmq_uninit), EBADF);

	CHECK_ERR(psmq_publish(NULL, "/t", NULL, 0), EINVAL);
	CHECK_ERR(psmq_publish(&psmq_uninit, "/t", NULL, 0), EBADF);
	CHECK_ERR(psmq_publish(&psmq, NULL, NULL, 0), EINVAL);
	CHECK_ERR(psmq_publish(&psmq, "t", NULL, 0), EBADMSG);
	psmqt_gen_random_string(buf, sizeof(buf));
	buf[0] = '/';
	CHECK_ERR(psmq_publish(&psmq, buf, NULL, 0), ENOBUFS);
	buf[PSMQ_MSG_MAX] = '\0';
	CHECK_ERR(psmq_publish(&psmq, buf, NULL, 0), ENOBUFS);
	buf[1] = 't';
	buf[2] = '\0';
	CHECK_ERR(psmq_publish(&psmq, buf, NULL, PSMQ_MSG_MAX), ENOBUFS);
	CHECK_ERR(psmq_publish(&psmq, buf, NULL, PSMQ_MSG_MAX - 2), ENOBUFS);

	CHECK_ERR(psmq_receive(NULL, &msg), EINVAL);
	CHECK_ERR(psmq_receive(&psmq_uninit, &msg), EBADF);
	CHECK_ERR(psmq_timedreceive(NULL, &msg, &tp), EINVAL);
	CHECK_ERR(psmq_timedreceive(&psmq_uninit, &msg, &tp), EBADF);
	CHECK_ERR(psmq_timedreceive_ms(NULL, &msg, 0), EINVAL);
	CHECK_ERR(psmq_timedreceive_ms(&psmq_uninit, &msg, 0), EBADF);

	CHECK_ERR(psmq_receive_prio(NULL, &msg, NULL), EINVAL);
	CHECK_ERR(psmq_receive_prio(&psmq_uninit, &msg, NULL), EBADF);
	CHECK_ERR(psmq_timedreceive_prio(NULL, &msg, NULL, &tp), EINVAL);
	CHECK_ERR(psmq_timedreceive_prio(&psmq_uninit, &msg, NULL, &tp), EBADF);
	CHECK_ERR(psmq_timedreceive_prio_ms(NULL, &msg, NULL, 0), EINVAL);
	CHECK_ERR(psmq_timedreceive_prio_ms(&psmq_uninit, &msg, NULL, 0), EBADF);

	CHECK_ERR(psmq_subscribe(NULL, "/t"), EINVAL);
	CHECK_ERR(psmq_subscribe(&psmq_uninit, "/t"), EBADF);

	CHECK_ERR(psmq_unsubscribe(NULL, "/t"), EINVAL);
	CHECK_ERR(psmq_unsubscribe(&psmq_uninit, "/t"), EBADF);

	/* tests that creates own custom set of
	 * clients, and only need broker to start/stop */
	mt_prepare_test = psmqt_prepare_test;
	mt_cleanup_test = psmqt_cleanup_test;

	mt_run(psmq_initialize);
	mt_run(psmq_initialize_queue_not_exist);
	mt_run(psmq_initialize_too_much_clients);

	/* tests that needs broker and use default set
	 * of clients for testing, one subscriber and
	 * one publisher */
	mt_prepare_test = psmqt_prepare_test_with_clients;
	mt_cleanup_test = psmqt_cleanup_test_with_clients;

	CHECK_ERR(psmq_receive(&gt_pub_psmq, NULL), EINVAL);
	CHECK_ERR(psmq_receive_prio(&gt_pub_psmq, NULL, NULL), EINVAL);

	CHECK_ERR(psmq_timedreceive(&gt_pub_psmq, NULL, &tp), EINVAL);
	CHECK_ERR(psmq_timedreceive(&gt_pub_psmq, &msg, NULL), EINVAL);
	CHECK_ERR(psmq_timedreceive_prio(&gt_pub_psmq, NULL, NULL, &tp), EINVAL);
	CHECK_ERR(psmq_timedreceive_prio(&gt_pub_psmq, &msg, NULL, NULL), EINVAL);
#ifndef __NetBSD__
	/* netbsd is supposed to return error here, but for some reasons
	 * it behaves as if we passed time = 0, and it returns immediately
	 * with ETIMEDOUT. Ignore this test on netbsd in that case */
	CHECK_ERR(psmq_timedreceive(&gt_pub_psmq, &msg, &tp_inval), EINVAL);
#endif
	CHECK_ERR(psmq_timedreceive_ms(&gt_pub_psmq, NULL, 0), EINVAL);

	CHECK_ERR(psmq_subscribe(&gt_pub_psmq, NULL), EINVAL);
	CHECK_ERR(psmq_subscribe(&gt_pub_psmq, ""), EINVAL);
	CHECK_ERR(psmq_subscribe(&gt_pub_psmq, "t"), EBADMSG);
	psmqt_gen_random_string(buf, sizeof(buf));
	buf[0] = '/';
	CHECK_ERR(psmq_subscribe(&gt_pub_psmq, buf), ENOBUFS);
	buf[PSMQ_MSG_MAX] = '\0';
	CHECK_ERR(psmq_subscribe(&gt_pub_psmq, buf), ENOBUFS);

	CHECK_ERR(psmq_unsubscribe(&gt_pub_psmq, NULL), EINVAL);
	CHECK_ERR(psmq_unsubscribe(&gt_pub_psmq, ""), EINVAL);
	CHECK_ERR(psmq_unsubscribe(&gt_pub_psmq, "t"), EBADMSG);
	psmqt_gen_random_string(buf, sizeof(buf));
	buf[0] = '/';
	CHECK_ERR(psmq_unsubscribe(&gt_pub_psmq, buf), ENOBUFS);
	buf[PSMQ_MSG_MAX] = '\0';
	CHECK_ERR(psmq_unsubscribe(&gt_pub_psmq, buf), ENOBUFS);

	CHECK_ERR(psmq_ioctl_reply_timeout(NULL, 10), EINVAL);
	CHECK_ERR(psmq_ioctl_reply_timeout(&psmq_uninit, 10), EBADF);
	CHECK_ERR(psmq_ioctl(&gt_sub_psmq, PSMQ_IOCTL_REPLY_TIMEOUT, USHRT_MAX + 1u),
			EINVAL);


	mt_run(psmq_unsub);
	mt_run(psmq_sub_max_topic);
	mt_run(psmq_sub_max_topic_plus_one);
	mt_run(psmq_sub_max_topic_minus_one);
	mt_run(psmq_pub_paylen_and_payload_null);
	mt_run(psmq_publish_receive);
	mt_run(psmq_publish_with_prio);
	mt_run(psmq_publish_timedreceive);
	mt_run(psmq_publish_timedreceive_ms);
	mt_run(psmq_sub_after_init);
	mt_run(psmq_unsub_after_init);
	mt_run(psmq_unsub_not_subscribed);
	mt_run_param(psmq_set_reply_timeout, 0);
	mt_run_param(psmq_set_reply_timeout, 100);
	mt_run_param(psmq_set_reply_timeout, USHRT_MAX - 1);
	mt_run_param(psmq_set_reply_timeout, USHRT_MAX);
}
