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
    mt_fok(psmq_init(&psmq, gt_broker_name, qname, 10));
    mt_fok(psmq_cleanup(&psmq));
    mq_unlink(qname);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_initialize_null_psmq(void)
{
    mt_ferr(psmq_init(NULL, "/b", "/q", 10), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_initialize_null_broker_name(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_ferr(psmq_init(&psmq, NULL, "/q", 10), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_initialize_null_queue_name(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_ferr(psmq_init(&psmq, "/b", NULL, 10), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_initialize_invalid_maxmsg(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_ferr(psmq_init(&psmq, "/b", "/q", -1), EINVAL);
    mt_ferr(psmq_init(&psmq, "/b", "/q", 0), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_initialize_bad_queue_name(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_ferr(psmq_init(&psmq, "b",  "/q", 10), EINVAL);
    mt_ferr(psmq_init(&psmq, "/b", "q", 10), EINVAL);
    mt_ferr(psmq_init(&psmq, "",   "/q", 10), EINVAL);
    mt_ferr(psmq_init(&psmq, "/b", "", 10), EINVAL);
    mq_unlink("/q");
}


/* ==========================================================================
   ========================================================================== */


static void psmq_initialize_queue_not_exist(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mq_unlink("/b");
    mt_ferr(psmq_init(&psmq, "/b",  "/q", 10), ENOENT);
    mq_unlink("/q");
}


/* ==========================================================================
   ========================================================================== */


static void psmq_initialize_queue_too_long(void)
{
    char         qname[PSMQ_PAYLOAD_MAX + 10];
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    psmqt_gen_queue_name(qname, sizeof(qname));
    mt_ferr(psmq_init(&psmq, "/b",  qname, 10), ENAMETOOLONG);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub(void)
{
    char         qname[QNAME_LEN];
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    psmqt_gen_queue_name(qname, sizeof(qname));
    mt_assert(psmq_init(&psmq, gt_broker_name, qname, 10) == 0);
    mt_fok(psmq_subscribe(&psmq, "/t"));
    mt_assert(psmq_cleanup(&psmq) == 0);
    mq_unlink(qname);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub_max_topic(void)
{
    char         topic[PSMQ_TOPIC_MAX + 1];
    char         qname[QNAME_LEN];
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    psmqt_gen_queue_name(qname, sizeof(qname));
    psmqt_gen_random_string(topic, sizeof(topic));
    topic[0] = '/';
    mt_assert(psmq_init(&psmq, gt_broker_name, qname, 10) == 0);
    mt_fok(psmq_subscribe(&psmq, topic));
    mt_assert(psmq_cleanup(&psmq) == 0);
    mq_unlink(qname);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub_null_topic(void)
{
    mt_ferr(psmq_subscribe(&gt_pub_psmq, NULL), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub_empty_topic(void)
{
    mt_ferr(psmq_subscribe(&gt_pub_psmq, ""), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub_too_long_topic(void)
{
    char  topic[PSMQ_TOPIC_MAX + 10];
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    psmqt_gen_random_string(topic, sizeof(topic));
    mt_ferr(psmq_subscribe(&gt_pub_psmq, topic), ENOBUFS);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub_uninitialized(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    memset(&psmq, 0x00, sizeof(psmq));
    mt_ferr(psmq_subscribe(&psmq, "/t"), EBADF);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_unsub_uninitialized(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    memset(&psmq, 0x00, sizeof(psmq));
    mt_ferr(psmq_unsubscribe(&psmq, "/t"), EBADF);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_unsub_empty_topic(void)
{
    mt_ferr(psmq_unsubscribe(&gt_pub_psmq, ""), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_unsub_too_long_topic(void)
{
    char  topic[PSMQ_TOPIC_MAX + 10];
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    psmqt_gen_random_string(topic, sizeof(topic));
    mt_ferr(psmq_unsubscribe(&gt_pub_psmq, topic), ENOBUFS);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_cleanup_uninitialized(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    memset(&psmq, 0x00, sizeof(psmq));
    mt_ferr(psmq_cleanup(&psmq), EBADF);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_pub_uninitialized(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    memset(&psmq, 0x00, sizeof(psmq));
    mt_ferr(psmq_publish(&psmq, "/t", NULL, 0, 0), EBADF);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_enable_uninitialized(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    memset(&psmq, 0x00, sizeof(psmq));
    mt_ferr(psmq_enable(&psmq, 1), EBADF);
    mt_ferr(psmq_disable_threaded(&psmq), EBADF);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_enable_invalid_value(void)
{
    mt_ferr(psmq_enable(&gt_pub_psmq, 2), EINVAL);
    mt_ferr(psmq_enable(&gt_pub_psmq, 10), EINVAL);
    mt_ferr(psmq_enable(&gt_pub_psmq, -1), EINVAL);
    mt_ferr(psmq_enable(&gt_pub_psmq, -10), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_receive_uninitialized(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    memset(&psmq, 0x00, sizeof(psmq));
    mt_ferr(psmq_receive(&psmq, psmqt_msg_receiver, NULL), EBADF);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_timedreceive_uninitialized(void)
{
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    memset(&psmq, 0x00, sizeof(psmq));
    mt_ferr(psmq_timedreceive_ms(&psmq, psmqt_msg_receiver, NULL, 100),
        EBADF);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub_null_psmq(void)
{
    mt_ferr(psmq_subscribe(NULL, "/t"), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_unsub_null_psmq(void)
{
    mt_ferr(psmq_unsubscribe(NULL, "/t"), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_unsub_not_subscribed(void)
{
    mt_ferr(psmq_unsubscribe(&gt_pub_psmq, "/x"), ENOENT);
}

/* ==========================================================================
   ========================================================================== */


static void psmq_cleanup_null_psmq(void)
{
    mt_ferr(psmq_cleanup(NULL), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_pub_null_psmq(void)
{
    mt_ferr(psmq_publish(NULL, "/t", NULL, 0, 0), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_pub_null_topic(void)
{
    mt_ferr(psmq_publish(&gt_pub_psmq, NULL, NULL, 0, 0), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_pub_too_big_topic(void)
{
    char  topic[PSMQ_TOPIC_MAX + 10];
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    psmqt_gen_random_string(topic, sizeof(topic));

    mt_ferr(psmq_publish(&gt_pub_psmq, topic, NULL, 0, 0), ENOBUFS);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_pub_too_big_message(void)
{
    char  payload[PSMQ_PAYLOAD_MAX + 10];
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    psmqt_gen_random_string(payload, sizeof(payload));
    mt_ferr(psmq_publish(&gt_pub_psmq, "/t", payload, sizeof(payload), 0),
        ENOBUFS);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_pub_paylen_and_payload_null(void)
{
    mt_fok(psmq_publish(&gt_pub_psmq, "/t", NULL, 1, 0));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_enable_null_psmq(void)
{
    mt_ferr(psmq_enable(NULL, 1), EINVAL);
    mt_ferr(psmq_disable_threaded(NULL), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_receive_null_psmq(void)
{
    mt_ferr(psmq_receive(NULL, psmqt_msg_receiver, NULL), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_timedreceive_null_psmq(void)
{
    mt_ferr(psmq_timedreceive(NULL, psmqt_msg_receiver, NULL, NULL), EINVAL);
    mt_ferr(psmq_timedreceive_ms(NULL, psmqt_msg_receiver, NULL, 1), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_unsub(void)
{
    char         qname[QNAME_LEN];
    struct psmq  psmq;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    psmqt_gen_queue_name(qname, sizeof(qname));
    mt_fok(psmq_init(&psmq, gt_broker_name, qname, 10));
    mt_fok(psmq_subscribe(&psmq, "/t"));
    mt_fok(psmq_unsubscribe(&psmq, "/t"));
    mt_fok(psmq_cleanup(&psmq));
    mq_unlink(qname);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_enable_disable(void)
{
    char             qname[QNAME_LEN];
    struct psmq      psmq;
    struct psmq_msg  msg;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    psmqt_gen_queue_name(qname, sizeof(qname));
    mt_fok(psmq_init(&psmq, gt_broker_name, qname, 10));
    mt_fok(psmq_subscribe(&psmq, "/t"));
    mt_fok(psmq_enable(&psmq, 1));
    mt_fok(psmq_enable(&psmq, 0));
    mt_fok(psmq_enable(&psmq, 1));
    mt_fok(psmq_disable_threaded(&psmq));
    mt_fok(psmq_receive(&psmq, psmqt_msg_receiver, &msg));
    mt_fail(strcmp(msg.topic, "-d") == 0);
    mt_fail(*(unsigned char*)msg.payload == 0);
    mt_fail(msg.paylen == 1);
    psmq.enabled = 0;
    mt_ferr(psmq_receive(&psmq, psmqt_msg_receiver, &msg), ENOTCONN);
    mt_fok(psmq_cleanup(&psmq));
    mq_unlink(qname);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_publish_receive(void)
{
    char             buf[PSMQ_PAYLOAD_MAX];
    struct psmq_msg  msg;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    psmqt_gen_random_string(buf, sizeof(buf));
    mt_fok(psmq_publish(&gt_pub_psmq, "/t", buf, sizeof(buf), 0));
    mt_fok(psmq_receive(&gt_sub_psmq, psmqt_msg_receiver, &msg));
    mt_fail(memcmp(msg.payload, buf, sizeof(buf)) == 0);
    mt_fail(strcmp(msg.topic, "/t") == 0);
    mt_fail(msg.paylen == sizeof(buf));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_publish_with_prio(void)
{

    char             buf0[PSMQ_PAYLOAD_MAX];
    char             buf1[PSMQ_PAYLOAD_MAX];
    char             buf2[PSMQ_PAYLOAD_MAX];
    struct psmq_msg  msg;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    psmqt_gen_random_string(buf0, sizeof(buf0));
    psmqt_gen_random_string(buf1, sizeof(buf1));
    psmqt_gen_random_string(buf2, sizeof(buf2));

    mt_fok(psmq_publish(&gt_pub_psmq, "/t", buf0, sizeof(buf0), 0));
    mt_fok(psmq_publish(&gt_pub_psmq, "/t", buf1, sizeof(buf1), 3));
    mt_fok(psmq_publish(&gt_pub_psmq, "/t", buf2, sizeof(buf2), 2));

    /* yield and wait a second so all messages with specific priorities
     * are sent
     */

    sleep(1);

    mt_fok(psmq_receive(&gt_sub_psmq, psmqt_msg_receiver, &msg));
    mt_fail(memcmp(msg.payload, buf1, sizeof(buf1)) == 0);
    mt_fok(psmq_receive(&gt_sub_psmq, psmqt_msg_receiver, &msg));
    mt_fail(memcmp(msg.payload, buf2, sizeof(buf2)) == 0);
    mt_fok(psmq_receive(&gt_sub_psmq, psmqt_msg_receiver, &msg));
    mt_fail(memcmp(msg.payload, buf0, sizeof(buf0)) == 0);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_publish_with_invalid_prio(void)
{
    mt_ferr(psmq_publish(&gt_pub_psmq, "/t", NULL, 0, UINT_MAX), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_publish_timedreceive(void)
{
    char             buf[PSMQ_PAYLOAD_MAX];
    struct psmq_msg  msg;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    psmqt_gen_random_string(buf, sizeof(buf));
    mt_fok(psmq_publish(&gt_pub_psmq, "/t", buf, sizeof(buf), 0));
    mt_fok(psmq_timedreceive_ms(&gt_sub_psmq, psmqt_msg_receiver, &msg, 999));
#if __QNX__ || __QNXNTO
    /* qnx (up to 6.4.0 anyway) has a bug, which causes mq_timedreceive
     * to return EINTR instead of ETIMEDOUT when timeout occurs
     */
    mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, psmqt_msg_receiver, &msg, 100),
        EINTR);
#else
    mt_ferr(psmq_timedreceive_ms(&gt_sub_psmq, psmqt_msg_receiver, &msg, 100),
        ETIMEDOUT);
#endif
    mt_fail(memcmp(msg.payload, buf, sizeof(buf)) == 0);
    mt_fail(strcmp(msg.topic, "/t") == 0);
    mt_fail(msg.paylen == sizeof(buf));
}


/* ==========================================================================
   ========================================================================== */


static void psmq_publish_timedreceive_wrong_timespec(void)
{
    struct timespec  tp;
    struct psmq_msg  msg;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    tp.tv_sec = 5;
    tp.tv_nsec = 1000000001l;
    mt_ferr(psmq_timedreceive(&gt_sub_psmq, NULL, &msg, &tp), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_publish_timedreceive_null_timespec(void)
{
    struct psmq_msg  msg;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_ferr(psmq_timedreceive(&gt_sub_psmq, NULL, &msg, NULL), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_sub_after_init(void)
{
    struct psmq_msg  msg;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    mt_fok(psmq_subscribe(&gt_sub_psmq, "/x"));
    mt_fok(psmq_receive(&gt_sub_psmq, psmqt_msg_receiver, &msg));
    mt_fail(strcmp(msg.topic, "-s") == 0);
    mt_fail(*(unsigned char*)msg.payload == 0);
    mt_fail(msg.paylen == 1);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_unsub_after_init(void)
{
    struct psmq_msg  msg;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    mt_fok(psmq_subscribe(&gt_sub_psmq, "/t"));
    mt_fok(psmq_receive(&gt_sub_psmq, psmqt_msg_receiver, &msg));
    mt_fail(strcmp(msg.topic, "-s") == 0);
    mt_fail(*(unsigned char*)msg.payload == 0);
    mt_fail(msg.paylen == 1);
}


/* ==========================================================================
   ========================================================================== */


static void psmq_initialize_too_much_clients(void)
{
    char         qname[PSMQ_MAX_CLIENTS + 2][QNAME_LEN];
    struct psmq  psmq[PSMQ_MAX_CLIENTS + 2];
    int          i;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    /* generate unique name for every queue
     */

    psmqt_gen_unique_queue_name_array(qname, PSMQ_MAX_CLIENTS + 2, QNAME_LEN);

    for (i = 0; i != PSMQ_MAX_CLIENTS; ++i)
    {
        mt_fail(psmq_init(&psmq[i], gt_broker_name, qname[i], 10) == 0);
    }

    /* now create 2 clients that won't fit into broker memory
     */

    mt_ferr(psmq_init(&psmq[i], gt_broker_name, qname[i], 10), ENOSPC);
    ++i;
    mt_ferr(psmq_init(&psmq[i], gt_broker_name, qname[i], 10), ENOSPC);

    for (i = 0; i != PSMQ_MAX_CLIENTS; ++i)
    {
        mt_fok(psmq_cleanup(&psmq[i]));
        mq_unlink(qname[i]);
    }

    /* now that slots are free, again try to connect those
     * two failed connections
     */

    mt_fail(psmq_init(&psmq[i], gt_broker_name, qname[i], 10) == 0);
    ++i;
    mt_fail(psmq_init(&psmq[i], gt_broker_name, qname[i], 10) == 0);

    --i;
    mt_fok(psmq_cleanup(&psmq[i]));
    mq_unlink(qname[i]);
    ++i;
    mt_fok(psmq_cleanup(&psmq[i]));
    mq_unlink(qname[i]);
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
    /* tests that creates own custom set of clients, and only need
     * broker to start/stop
     */

    mt_prepare_test = psmqt_prepare_test;
    mt_cleanup_test = psmqt_cleanup_test;

    mt_run(psmq_cleanup_null_psmq);
    mt_run(psmq_cleanup_uninitialized);
    mt_run(psmq_enable_disable);
    mt_run(psmq_enable_null_psmq);
    mt_run(psmq_enable_uninitialized);
    mt_run(psmq_initialize);
    mt_run(psmq_initialize_bad_queue_name);
    mt_run(psmq_initialize_invalid_maxmsg);
    mt_run(psmq_initialize_null_broker_name);
    mt_run(psmq_initialize_null_psmq);
    mt_run(psmq_initialize_null_queue_name);
    mt_run(psmq_initialize_queue_not_exist);
    mt_run(psmq_initialize_queue_too_long);
    mt_run(psmq_initialize_too_much_clients);
    mt_run(psmq_pub_null_psmq);
    mt_run(psmq_pub_uninitialized);
    mt_run(psmq_receive_null_psmq);
    mt_run(psmq_receive_uninitialized);
    mt_run(psmq_sub);
    mt_run(psmq_sub_null_psmq);
    mt_run(psmq_timedreceive_null_psmq);
    mt_run(psmq_timedreceive_uninitialized);
    mt_run(psmq_unsub);
    mt_run(psmq_unsub_null_psmq);
    mt_run(psmq_sub_max_topic);

    /* tests that needs broker and use default set of clients for
     * testing, one subscriber and one publisher
     */

    mt_prepare_test = psmqt_prepare_test_with_clients;
    mt_cleanup_test = psmqt_cleanup_test_with_clients;

    mt_run(psmq_enable_invalid_value);
    mt_run(psmq_pub_null_topic);
    mt_run(psmq_pub_paylen_and_payload_null);
    mt_run(psmq_pub_too_big_message);
    mt_run(psmq_pub_too_big_topic);
    mt_run(psmq_publish_receive);
    mt_run(psmq_publish_with_prio);
    mt_run(psmq_publish_with_invalid_prio);
    mt_run(psmq_publish_timedreceive);
    mt_run(psmq_publish_timedreceive_wrong_timespec);
    mt_run(psmq_publish_timedreceive_null_timespec);
    mt_run(psmq_sub_after_init);
    mt_run(psmq_sub_empty_topic);
    mt_run(psmq_sub_null_topic);
    mt_run(psmq_sub_too_long_topic);
    mt_run(psmq_sub_uninitialized);
    mt_run(psmq_unsub_after_init);
    mt_run(psmq_unsub_empty_topic);
    mt_run(psmq_unsub_not_subscribed);
    mt_run(psmq_unsub_too_long_topic);
    mt_run(psmq_unsub_uninitialized);
}
