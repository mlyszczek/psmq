/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         ----------------------------------------------------------
        / this file contains all public functions of the psmq      \
        | library. Library is to make communication between client |
        \ and the broker an easy peasy task.                       /
         ----------------------------------------------------------
           \
            \
              _____   _________
             /     \_/         |
            |                 ||
            |                 ||
           |    ###\  /###   | |
           |     0  \/  0    | |
          /|                 | |
         / |        <        |\ \
        | /|                 | | |
        | |     \_______/   |  | |
        | |                 | / /
        /||                 /|||
           ----------------|
                | |    | |
                ***    ***
               /___\  /___\
   ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "psmq-common.h"
#include "psmq.h"
#include "valid.h"


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    Processes ack reply from the broker

    returns:
           >0       errno message from failed ack from broker
            0       broker send successfull ack
           -2       received something other than ack
           -3       malformed data received from broker
   ========================================================================== */


static int psmq_ack
(
    char         *topic,     /* topic of received ack */
    void         *payload,   /* payload of ack response */
    size_t        paylen,    /* length of payload buffer */
    unsigned int  prio,      /* not used */
    void         *userdata   /* not used */
)
{
    (void)userdata;
    (void)prio;

    if (topic[0] != '-')
    {
        /* ack messages are sent only when request starting with '-'
         * character
         */

        return -2;
    }

    if (paylen != 1)
    {
        /* we expect to receive int here with ack, nothing else
         */

        errno = EBADMSG;
        return -3;
    }

    return *(unsigned char *)payload;
}


/* ==========================================================================
    Processes ack reply from the broker from "-o" (open) message. That
    reply, beside ack, contains also file descriptor used in further
    communication with broker. When return value is 0, fd will be stored
    in userdata location.

    returns:
           >0       errno message from failed ack from broker
            0       broker send successfull ack
           -2       received something other than open ack
           -3       malformed data received from broker
   ========================================================================== */


static int psmq_open_ack
(
    char         *topic,     /* topic of received ack */
    void         *payload,   /* payload of ack response */
    size_t        paylen,    /* length of payload buffer */
    unsigned int  prio,      /* not used */
    void         *userdata   /* file descriptor to use when talking with broker */
)
{
    (void)prio;

    if (strcmp(topic, PSMQ_TOPIC_OPEN) != 0)
    {
        /* open response is suppose to send back message with
         * register topic, that is not the case here. There may be
         * some old data in queue. Return -2, so caller can decide
         * what to do with it.
         */

        return -2;
    }

    if (paylen != 2)
    {
        /* we expect to receive 2 chars here, one for ack, and one
         * with file descriptor
         */

        errno = EBADMSG;
        return -3;
    }

    *(unsigned char *)userdata = *((unsigned char *)payload + 1);
    return *(unsigned char *)payload;
}


/* ==========================================================================
    This is same as psmq_receive() but does not check for psmq->enabled flag
   ========================================================================== */


static int psmq_receive_internal
(
    struct psmq     *psmq,     /* psmq object */
    psmq_sub_clbk    clbk,     /* function to be called with received data */
    void            *userdata  /* user data passed to clkb */
)
{
    struct psmq_msg  msg;      /* buffer to receive message */
    unsigned int     prio;     /* message priority */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    VALID(EINVAL, psmq);
    VALID(EINVAL, clbk);
    VALID(EBADF, psmq->qsub != (mqd_t)-1);
    VALID(EBADF, psmq->qsub != psmq->qpub);

    if (mq_receive(psmq->qsub, (char *)&msg, sizeof(msg), &prio) == -1)
    {
        return -1;
    }

    return clbk(msg.topic, msg.payload, msg.paylen, prio, userdata);
}


/* ==========================================================================
    Subscribes or unsubsribes from specific 'topic' If 'psmq' is not
    enabled, function will also check for subscribe ack, otherwise caller is
    responsible for reading it in main application.

    Returns 0 on success or -1 on error
   ========================================================================== */


static int psmq_un_subscribe
(
    struct psmq  *psmq,     /* psmq object */
    const char   *topic,    /* topic to unsubsribe from */
    int           sub       /* subscribe - 1 or unsubsribe - 0 */
)
{
    char          topicfd[PSMQ_TOPIC_WITH_FD + 1]; /* topic with fd info */
    int           ack;      /* response from the broker */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    VALID(EINVAL, psmq);
    VALID(EINVAL, topic);
    VALID(EINVAL, topic[0] != '\0');
    VALID(EBADF, psmq->qsub != (mqd_t)-1);
    VALID(EBADF, psmq->qsub != psmq->qpub);
    VALID(ENOBUFS, strlen(topic) <= PSMQ_TOPIC_MAX);

    /* send subscribe request to the server
     */

    sprintf(topicfd, sub ? PSMQ_TOPIC_SUBSCRIBE"/%d" :
        PSMQ_TOPIC_UNSUBSCRIBE"/%d", psmq->fd);

    if (psmq_publish(psmq, topicfd, topic, strlen(topic) + 1, 0) != 0)
    {
        /* failed to send message over mqueue
         */

        return -1;
    }

    if (psmq->enabled)
    {
        /* psmq is enabled, that means it is primed to receive
         * normal messages, so we cannot check for subscribe ack,
         * as we could read normal message addressed to main
         * application. In this mode user is responsible for
         * reading ack - we return success here.
         */

        return 0;
    }

    /* now wait for ack reply
     */

    ack = psmq_receive_internal(psmq, psmq_ack, NULL);

    if (ack != 0)
    {
        if (ack > 0)
        {
            errno = ack;
        }

        return -1;
    }

    return 0;
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
    Publishes message on 'topic' with 'payload' of size 'paylen' with 'prio'
    priority to broker defined in 'psmq'.

    Returns 0 on success or -1 on errors

    errno:
            EINVAL      psmq is invalid (null)
            EBADF       psmq was not properly initialized
            ENOBUFS     topic or payload is to big to fit into buffers
   ========================================================================== */


int psmq_publish
(
    struct psmq         *psmq,     /* psmq object */
    const char          *topic,    /* topic of message to be sent */
    const void          *payload,  /* payload of message to be sent */
    size_t               paylen,   /* length of payload buffer */
    unsigned int         prio      /* message priority */
)
{
    struct psmq_msg_pub  pub;      /* buffer used to send out data to broker */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    VALID(EINVAL, psmq);
    VALID(EBADF, psmq->qpub != (mqd_t)-1);
    VALID(EBADF, psmq->qsub != psmq->qpub);
    VALID(EINVAL, topic);
    VALID(ENOBUFS, strlen(topic) < sizeof(pub.topic));
    VALID(ENOBUFS, paylen <= sizeof(pub.payload));
    VALID(EBADMSG, topic[0] == '/' || topic[0] == '-');

    if (topic[0] != '-')
    {
        /* when publishing message that is different than control
         * messages, we need to check if those messages will fit
         * message struct on clients. It is because struct for
         * publish messages may be bigger than struct for receiving
         * messages, and thus it is possible that message will
         * reach broker with no problem, but then broker won't be
         * able to propage it to the clients, because receive
         * struct is not big enough
         */

        VALID(ENOBUFS, strlen(topic) < size_of_member(struct psmq_msg, topic));
        VALID(ENOBUFS, paylen <= size_of_member(struct psmq_msg, payload));
    }

    memset(&pub, 0x00, sizeof(pub));
    strcpy(pub.topic, topic);
    pub.paylen = 0;

    /* payload may be NULL and it's ok, so set payload only if it
     * was set
     */

    if (payload)
    {
        memcpy(pub.payload, payload, paylen);
        pub.paylen = paylen;
    }

    return mq_send(psmq->qpub, (const char *)&pub, sizeof(pub), prio);
}


/* ==========================================================================
    Waits for data to be sent from broker 'psmq' and calls user's 'clbk'
    with optional 'userdata' when data is received.

    When error ocurs, function returns -1, when message has been read from
    queue with success, user's clbk() function is returned and return value
    from that function is returned.

    errno:
            EINVAL      psmq is invalid (null)
            EINVAL      clbk is invalid (null)
            EBADF       psmq was not properly initialized
            ENOTCONN    psmq object is not enabled and cannot receive msgs
   ========================================================================== */


int psmq_receive
(
    struct psmq   *psmq,     /* psmq object */
    psmq_sub_clbk  clbk,     /* function to be called with received data */
    void          *userdata  /* user data passed to clkb */
)
{
    VALID(EINVAL, psmq);
    VALID(EBADF, psmq->qpub != (mqd_t)-1);
    VALID(EBADF, psmq->qsub != psmq->qpub);
    VALID(ENOTCONN, psmq->enabled);
    return psmq_receive_internal(psmq, clbk, userdata);
}


/* ==========================================================================
    Same as psmq_receive(), but block only for 'miliseconds' time and not
    forever like in psmq_receive()

    When timeout occurs, clbk() is not called.
    When seconds is 0, function returns immediately.

    errno:
            EINVAL      psmq is invalid (null)
            EINVAL      clbk is invalid (null)
            EBADF       psmq was not properly initialized
            ENOTCONN    psmq object is not enabled and cannot receive msgs
            ETIMEDOUT   timeout occured and message did not arrive

    notes:
            On qnx (up until 6.4.0 at least) when timeout occurs,
            mq_timedreceveice will return EINTR instead of ETIMEDOUT
   ========================================================================== */


int psmq_timedreceive
(
    struct psmq     *psmq,     /* psmq object */
    psmq_sub_clbk    clbk,     /* function to be called with received data */
    void            *userdata, /* user data passed to clkb */
    struct timespec *tp        /* absolute point in time when timeout occurs */
)
{
    struct psmq_msg  msg;      /* buffer to receive message */
    unsigned int     prio;     /* received message priority */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    VALID(EINVAL, psmq);
    VALID(EINVAL, clbk);
    VALID(EBADF, psmq->qsub != (mqd_t)-1);
    VALID(EBADF, psmq->qsub != psmq->qpub);
    VALID(ENOTCONN, psmq->enabled);

    if (mq_timedreceive(psmq->qsub, (char *)&msg, sizeof(msg), &prio, tp) == -1)
    {
        return -1;
    }

    return clbk(msg.topic, msg.payload, msg.paylen, prio, userdata);
}


/* ==========================================================================
    Same as psmq_timedreceive() but accepts 'milisecond' time that shall
    pass before timeout should occur instead of absolute timespec.
   ========================================================================== */


int psmq_timedreceive_ms
(
    struct psmq     *psmq,     /* psmq object */
    psmq_sub_clbk    clbk,     /* function to be called with received data */
    void            *userdata, /* user data passed to clkb */
    size_t           ms        /* ms to wait until timeout occurs */
)
{
    struct timespec  tp;       /* absolute point in time when timeout occurs */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* no VALID() checks, since all parameters are validated in
     * psmqd_timedreceive() function
     */

    tp.tv_sec = 0;
    tp.tv_nsec = 0;

    if (ms)
    {
        size_t  nsec;  /* number of nanoseconds to add to timer */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        /* milisecond is set, configure tp to wait this ammount of time
         * instead of returning immediately
         */

        clock_gettime(CLOCK_REALTIME, &tp);

        nsec = ms % 1000;
        nsec *= 1000000l;

        tp.tv_sec += ms / 1000;
        tp.tv_nsec += nsec;

        if (tp.tv_nsec >= 1000000000l)
        {
            /* overflow on nsec part */

            tp.tv_nsec -= 1000000000l;
            tp.tv_sec += 1;
        }
    }

    return psmq_timedreceive(psmq, clbk, userdata, &tp);
}


/* ==========================================================================
    Create mqueue for receiving and opens connection to broker

    Return 0 when broker sends back connection confirmation or -1 when error
    occured.
   ========================================================================== */


int psmq_init
(
    struct psmq     *psmq,        /* psmq object to initialize */
    const char      *brokername,  /* name of the broker to connect to */
    const char      *mqname,      /* name of the reciving queue to create */
    int              maxmsg       /* max queued messages in mqname */
)
{
    struct mq_attr   mqa;         /* mqueue attributes */
    int              ack;         /* ACK from the broker after open */
    size_t           mqnamelen;   /* length of mqname string */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    VALID(EINVAL, psmq);
    VALID(EINVAL, brokername);
    VALID(EINVAL, brokername[0] == '/');
    VALID(EINVAL, mqname);
    VALID(EINVAL, mqname[0] == '/');
    VALID(EINVAL, maxmsg > 0);
    mqnamelen = strlen(mqname);
    VALID(ENAMETOOLONG, mqnamelen < size_of_member(struct psmq_msg_pub,
        payload));


    memset(psmq, 0x00, sizeof(struct psmq));
    mqa.mq_msgsize = sizeof(struct psmq_msg);
    mqa.mq_maxmsg = maxmsg;
    psmq->qsub = (mqd_t)-1;
    psmq->qpub = (mqd_t)-1;

    /* if staled queue exist, remove it, so it doesn't do weird
     * stuff
     */

    mq_unlink(mqname);

    /* open subscribe queue, where we will receive data we
     * subscribed to, and at the beginning is used to report
     * error from broker to client
     */

    psmq->qsub = mq_open(mqname, O_RDONLY | O_CREAT, 0600, &mqa);
    if (psmq->qsub == (mqd_t)-1)
    {
        return -1;
    }

    /* open publish queue, this will be used to subscribed to
     * topics at the start, and later this will be used to publish
     * data on given topic
     */

    psmq->qpub = mq_open(brokername, O_WRONLY);
    if (psmq->qpub == (mqd_t)-1)
    {
        mq_close(psmq->qsub);
        psmq->qsub = (mqd_t)-1;
        return -1;
    }

    /* both queues have been created, that means we have enough
     * memory to operate, now we need to register to broker, so it
     * knows where to send messages
     */

    if (psmq_publish(psmq, PSMQ_TOPIC_OPEN, mqname, mqnamelen + 1, 0) != 0)
    {
        goto error;
    }

    /* check response from the broker on subscribe queue to check
     * if broker managed to allocate memory for us and open queue
     * on his side. Read from broker until we read something else
     * than -2, which means we read data other than register ack
     * from the broker. This may happen when we open queue that
     * already has some outstanding data - like from previous
     * crashed session, we can safely discard those messages.
     */

    while ((ack = psmq_receive_internal(psmq, psmq_open_ack, &psmq->fd)) == -2)
        {};

    /* broker will return either 0 or errno to indicate what
     * went wrong on his side, if ack is 0, broker will send
     * file descriptor to use when communicating with him.
     */

    if (ack != 0)
    {
        if (ack > 0)
        {
            errno = ack;
        }

        goto error;
    }

    /* ack received and fd is valid, we are victorious
     */

    return 0;

error:
    mq_close(psmq->qpub);
    mq_close(psmq->qsub);
    psmq->qpub = (mqd_t)-1;
    psmq->qsub = (mqd_t)-1;
    return -1;
}


/* ==========================================================================
    Cleans up whatever has been allocate through the life cycle of 'psmq'
   ========================================================================== */


int psmq_cleanup
(
    struct psmq  *psmq  /* psmq object to cleanup */
)
{
    char          topicfd[PSMQ_TOPIC_WITH_FD + 1]; /* topic with fd info */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    VALID(EINVAL, psmq);
    VALID(EBADF, psmq->qsub != psmq->qpub);

    /* send close() to the broker, we don't care if it succed or not,
     * we close our booth and nothing can stop us from doing it
     */

    sprintf(topicfd, PSMQ_TOPIC_CLOSE"/%d", psmq->fd);
    psmq_publish(psmq, topicfd, NULL, 0, 0);
    mq_close(psmq->qpub);
    mq_close(psmq->qsub);
    psmq->qpub = (mqd_t) -1;
    psmq->qsub = (mqd_t) -1;
    return 0;

}


/* ==========================================================================
    Subscribes to 'psmq' broker on 'topic'. If 'psmq' is not enabled,
    function will also check for subscribe ack, otherwise caller is
    responsible for reading it in main application.

    Returns 0 on success or -1 on error
   ========================================================================== */


int psmq_subscribe
(
    struct psmq      *psmq,     /* psmq object */
    const char       *topic     /* topic to register to */
)
{
    return psmq_un_subscribe(psmq, topic, 1);
}


/* ==========================================================================
    Subscribes to 'psmq' broker on 'topic'. If 'psmq' is not enabled,
    function will also check for subscribe ack, otherwise caller is
    responsible for reading it in main application.

    Returns 0 on success or -1 on error
   ========================================================================== */


int psmq_unsubscribe
(
    struct psmq      *psmq,     /* psmq object */
    const char       *topic     /* topic to register to */
)
{
    return psmq_un_subscribe(psmq, topic, 0);
}


/* ==========================================================================
    Enables client for data receiving. Data will not be sent to client when
    this is not called after subscribing for topics. When client needs a
    break, you can call this function with enable set to 0. After that
    you again, won't be able to receive any messages. Calling this doesn't
    free nor allocate any resources on the broker side.
   ========================================================================== */


int psmq_enable
(
    struct psmq  *psmq,   /* psmq object */
    int           enable  /* enable or disable client */
)
{
    char          topicfd[PSMQ_TOPIC_WITH_FD + 1]; /* topic with fd info */
    int           ack;    /* response from the broker */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    VALID(EINVAL, psmq);
    VALID(EBADF, psmq->qsub != (mqd_t)-1);
    VALID(EBADF, psmq->qsub != psmq->qpub);
    VALID(EINVAL, (enable & ~1) == 0);

    /* send subscribe request to the server
     */

    sprintf(topicfd, enable ? PSMQ_TOPIC_ENABLE"/%d" : PSMQ_TOPIC_DISABLE"/%d",
        psmq->fd);

    if (psmq_publish(psmq, topicfd, NULL, 0, 0) != 0)
    {
        /* failed to send message over mqueue
         */

        return -1;
    }

    /* now wait for ack reply
     */

    ack = psmq_receive_internal(psmq, psmq_ack, NULL);

    if (ack != 0)
    {
        if (ack > 0)
        {
            errno = ack;
        }

        return -1;
    }

    psmq->enabled = enable;
    return 0;
}


/* ==========================================================================
    psmq is single-threaded by design, but user might want to have one
    thread that will receive messages and another that controls psmq. In
    such configuration it is impossible to send disable message and check
    for ack. It is possible that T1 will block in psmq_receive(), then
    another thread calls psmq_disable() and instead of psmq_disable(), ack
    with disable will be received by T1 that waits for message in
    psmq_receive() and psmq_disable() thread will block causing deadlock.
    For such situations exists this function. User can call this instead,
    and his receive function will receive disable ack and we simply exit
    after publish. It's up to user to later set psmq->enabled to 0, and
    provide proper synchronization.
   ========================================================================== */


int psmq_disable_threaded
(
    struct psmq  *psmq   /* psmq object */
)
{
    char          topicfd[PSMQ_TOPIC_WITH_FD + 1]; /* topic with fd info */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    VALID(EINVAL, psmq);
    VALID(EBADF, psmq->qsub != (mqd_t)-1);
    VALID(EBADF, psmq->qsub != psmq->qpub);

    /* send disable request to the server
     */

    sprintf(topicfd, PSMQ_TOPIC_DISABLE"/%d", psmq->fd);

    if (psmq_publish(psmq, topicfd, NULL, 0, 0) != 0)
    {
        /* failed to send message over mqueue
         */

        return -1;
    }

    /* threaded disable, caller is reponsible of setting
     * psmq->enabled to 0
     */

    return 0;
}
