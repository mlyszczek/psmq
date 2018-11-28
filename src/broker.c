/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    uthor: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         -------------------------------------------------------------
        / Broker modules is reponsible for processing events from the \
        | clients. This is where clients are initialized. subscribed. |
        | enabled and all published messages are propagaded to        |
        \ subscribed clients                                          /
         -------------------------------------------------------------
             \                 / \  //\
              \ |\___/|      /   \//  \\
                /0  0  \__  /    //  | \ \
               /     /  \/_/    //   |  \  \
               @_^_@'/   \/_   //    |   \   \
               //_^_/     \/_ //     |    \    \
            ( //) |        \///      |     \     \
          ( / /) _|_ /   )  //       |      \     _\
        ( // /) '/,_ _ _/  ( ; -.    |    _ _\.-~        .-~~~^-.
      (( / / )) ,-{        _      `-.|.-~-.           .~         `.
     (( // / ))  '/\      /                 ~-. _ .-~      .-~^-.  \
     (( /// ))      `.   {            }                   /      \  \
      (( / ))     .----~-.\        \-'                 .~         \  `. \^-.
                 ///.----..>        \             _ -~             `.  ^-`  ^-_
                   ///-._ _ _ _ _ _ _}^ - - - - ~                     ~-- ,.-~
                                                                      /.-~
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

#include <assert.h>
#include <embedlog.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mqueue.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "cfg.h"
#include "globals.h"
#include "psmq-common.h"
#include "topic-list.h"
#include "valid.h"


/* ==========================================================================
                  _                __           __
    ____   _____ (_)_   __ ____ _ / /_ ___     / /_ __  __ ____   ___   _____
   / __ \ / ___// /| | / // __ `// __// _ \   / __// / / // __ \ / _ \ / ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / /_ / /_/ // /_/ //  __/(__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/   \__/ \__, // .___/ \___//____/
/_/                                              /____//_/
   ========================================================================== */


/* structure describing connected client
 */

struct client
{
    /* descriptor of client's message queue, if this is (mqd_t)-1,
     * then that means client is not connected
     */

    mqd_t             mq;

    /* list of topics client is subscribed to, if NULL, client is
     * not subscribed to any topic
     */

    struct psmqd_tl  *topics;

    /* Defines whether client is enabled or not. When set to 0,
     * client won't receive any published messages (it will still
     * receive control messages though) even if he is subscribed
     * to published topic.
     */

    int               enabled;
};


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


#define EL_OPTIONS_OBJECT &g_psmqd_log
static mqd_t          qctrl;  /* mqueue handle to broker main control queue */
static struct client  clients[PSMQ_MAX_CLIENTS]; /* array of clients */


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    Finds first free slot in clients variable
   ========================================================================== */


static int psmqd_broker_get_free_client(void)
{
    int  fd;  /* number of free slot in clients */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    for (fd = 0; fd != PSMQ_MAX_CLIENTS; ++fd)
    {
        if (clients[fd].mq == (mqd_t)-1)
        {
            /* mq is not set, so this slot is available
             */

            return fd;
        }
    }

    /* all slots are used
     */

    return -1;
}


/* ==========================================================================
    Checks if published topic matches subscribed topic

    Return 0 when there is NO match, and 1 when they matches.
   ========================================================================== */


static int psmqd_broker_topic_matches
(
    const char  *pub_topic,  /* topic message was publish with */
    const char  *sub_topic   /* client's subscribe topic (may contain * or + */
)
{
    char         pubt[PSMQ_TOPIC_MAX + 1];  /* copy of pub_topic */
    char         subt[PSMQ_TOPIC_MAX + 1];  /* copy of sub_topic */
    char        *pubts;      /* saveptr of pubt for strtok_r */
    char        *subts;      /* saveptr of subt for strtok_r */
    char        *pubtok;     /* current token of pub topic */
    char        *subtok;     /* current token of sub topic */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    if (strcmp(pub_topic, sub_topic) == 0)
    {
        /* topics are equal, no need to parse them, quickly return
         */

        return 1;
    }

    strcpy(pubt, pub_topic);
    strcpy(subt, sub_topic);

    /* first tokenize will always be valid, since all topics must
     * start from '/' and this is checked before this function is
     * called
     */

    pubtok = strtok_r(pubt, "/", &pubts);
    subtok = strtok_r(subt, "/", &subts);

    while(pubtok && subtok)
    {
        /* let's iterate through all part of the topic
         */

        if (strcmp(subtok, "*") == 0)
        {
            /* path is equal up until now, and current token is '*'
             * so we can stop parsing now and treat topics as
             * matching.
             *
             * Examples of matching topics:
             *
             *      pub: /a/b/c/d
             *      sub: /a/b/*
             *
             *      pub: /a/b/c/d
             *      sub: /a/*
             *
             *      pub: /a/b/c/d
             *      sub: /*
             */

            return 1;
        }

        if (strcmp(subtok, "+") == 0)
        {
            /* path is equal up intil now, and current token is '+'
             * so we treat whole token as valid and continue for the
             * next one.
             *
             * Examples of matching topics:
             *
             *      pub: /a/b/c/d
             *      sub: /+/b/c/d
             *
             *      pub: /a/b/c/d
             *      sub: /a/b/+/d
             *
             *      pub: /a/b/c/d
             *      sub: /a/b/c/+
             */

            pubtok = strtok_r(NULL, "/", &pubts);
            subtok = strtok_r(NULL, "/", &subts);
            continue;
        }

        if (strcmp(subtok, pubtok) != 0)
        {
            /* tokens are not equal, topics doesn't match
             */

            return 0;
        }

        /* tokens are valid, take another token and continue with
         * the checks,
         */

        pubtok = strtok_r(NULL, "/", &pubts);
        subtok = strtok_r(NULL, "/", &subts);
    }

    /* we get here when we read all tokenes from either pub topic
     * or sub topic
     */

    if (subtok == pubtok)
    {
        /* both sub topic and pub topic were fully tokenized and
         * they match, topics are equal
         */

        return 1;
    }

    /* only one of the sub topic or pub topic was tokenized fully,
     * so these topics does not match
     */

    return 0;
}


/* ==========================================================================
    Sends message to the client with 'mq' mqueue

    On error -1 is returned and client session is closed. On success 0 will
    be returned.
   ========================================================================== */


static int psmqd_broker_reply_mq
(
    mqd_t            mq,       /* mqueue of client to send message to */
    const char      *topic,    /* topic of the message */
    const void      *payload,  /* data to send to the client */
    size_t           paylen,   /* length of payload to send */
    unsigned int     prio      /* message priority */
)
{
    struct psmq_msg  msg;      /* structure with message to send */
    struct timespec  tp;       /* absolute time when mq_send() call expire */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    memset(&msg, 0x00, sizeof(msg));
    strcpy(msg.topic, topic);
    memcpy(msg.payload, payload, paylen);
    msg.paylen = paylen;

    /* wait up to 100ms for client to free space in his mqueue
     */

    clock_gettime(CLOCK_REALTIME, &tp);
    tp.tv_nsec += 100000000l;

    if (tp.tv_nsec >= 1000000000l)
    {
        tp.tv_sec += 1;
        tp.tv_nsec -= 1000000000l;
    }

    return mq_timedsend(mq, (const char *)&msg, sizeof(msg), prio, &tp);
}


/* ==========================================================================
    Same as psmqd_broker_reply_mq() but accepts fd instead of mqueue
   ========================================================================== */


static int psmqd_broker_reply
(
    int           fd,       /* fd of client to send message to */
    const char   *topic,    /* topic of the message */
    const void   *payload,  /* data to send to the client */
    size_t        paylen,   /* length of payload to send */
    unsigned int  prio      /* message priority */
)
{
    return psmqd_broker_reply_mq(clients[fd].mq, topic, payload, paylen, prio);
}


/* ==========================================================================
    Extracts file decriptor information from msg->topic.

    Returns valid fd or UCHAR_MAX on errors.
   ========================================================================== */


static unsigned char psmqd_broker_get_fd
(
    struct psmq_msg_pub  *msg      /* message to read fd from */
)
{
    long                  fd;      /* fd converted from string */
    char                 *fdstr;   /* received fd as string */
    char                 *endptr;  /* error indicator for strol */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* all control frames must have topic in form "-X/NNN", where X is
     * command character, and NNN is clients file descriptor
     */

    fdstr = msg->topic + 3;

    /* convert string file descriptor into int
     */

    fd = strtol(fdstr, &endptr, 10);

    if (*endptr != '\0')
    {
        /* there was an unexpected character in fdstr
         */

        el_oprint(OELW, "received wrong file descriptor %s", fdstr);
        return UCHAR_MAX;
    }

    if (fd < 0 || fd >= PSMQ_MAX_CLIENTS)
    {
        /* file descriptor out of range
         */

        el_oprint(OELW, "received out of range file descriptor %s", fdstr);
        return UCHAR_MAX;
    }

    return (unsigned char)fd;
}


/* ==========================================================================
                                                       __
                _____ ___   ____ _ __  __ ___   _____ / /_ _____
               / ___// _ \ / __ `// / / // _ \ / ___// __// ___/
              / /   /  __// /_/ // /_/ //  __/(__  )/ /_ (__  )
             /_/    \___/ \__, / \__,_/ \___//____/ \__//____/
                            /_/
   ========================================================================== */


/* ==========================================================================
    Open connection with the broker
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    request:
            topic       str     "-o"
            payload     str     queue name where messages will be sent

    response:
            topic       str     "-o"
            payload
                error   uchar   0 no error or errno value
                fd      uchar   file descriptor to use when communicating
                                field is valid only when error is 0

    note:
            yes, errno is int, so max value of errno is 32767, but this is
            designed for embedded with small memory footprint, and mqueue
            always allocates all memory for all possible messages on the
            queue, so it is by design, to keep these messages as small as
            possible, thus errno can be max 255. Sometimes this may bite,
            when errno is bigger than 255. In such case, any errno bigger
            than 255 will get truncated to 255.

            This will also limit max clients up to 255. Yeah, like someone
            is going to have 255 different processes with different psmq
            clients in their embedded systems. Btw, I know someone will hit
            this. If you happened to do so, you can freely email me and
            laugh into my face for such riddiculus limitation.
   ========================================================================== */


static int psmqd_broker_open
(
    char          *qname    /* name of the queue to communicate with client */
)
{
    mqd_t          qc;      /* new communication queue */
    int            fd;      /* new file descriptor for the client */
    unsigned char  buf[2];  /* buffer with response */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* open communication line with client
     */

    qc = mq_open(qname, O_RDWR);
    if (qc == (mqd_t)-1)
    {
        /* couldn't open queue provided by client and thus we have
         * no way of informing him about error, so we only log this
         * and abort this session
         */

        el_operror(OELC, "open failed: mq_open(%s)", qname);
        return -1;
    }

    fd = psmqd_broker_get_free_client();
    if (fd == -1)
    {
        /* all slots are taken, send one int with error information
         * to the client
         */

        buf[0] = (unsigned char)ENOSPC;
        buf[1] = 0;
        el_oprint(OELW, "open failed client %s: no free slots", qname);
        psmqd_broker_reply_mq(qc, PSMQ_TOPIC_OPEN, buf, sizeof(buf), 0);
        mq_close(qc);
        return -1;
    }

    clients[fd].mq = qc;

    /* we have free slot and all data has been allocated, send
     * client file descriptor he can use to control communication
     */

    buf[0] = 0;
    buf[1] = fd;

    psmqd_broker_reply_mq(qc, PSMQ_TOPIC_OPEN, buf, sizeof(buf), 0);
    el_oprint(OELN, "[%3d] opened %s", fd, qname);
    return 0;
}


/* ==========================================================================
    Subscribes to topic in message payload
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    request:
            topic       str     "-s/NNN" where NNN is file descriptor
            payload     str     topic to subscribe to

    response
            topic       str     "-s"
            payload     uchar   0 - subscribed, or errno value

    errno for response:
            EBADMSG     payload is not a string
            UCHAR_MAX   returned errno from system is bigger than UCHAR_MAX
   ========================================================================== */


static int psmqd_broker_subscribe
(
    struct psmq_msg_pub  *msg      /* subscription request */
)
{
    unsigned char         fd;      /* clients file descriptor */
    unsigned char         err;     /* errno value to send to client */
    char                 *stopic;  /* subscribe topic from client */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    stopic = (char *)msg->payload;
    fd = psmqd_broker_get_fd(msg);
    if (fd == UCHAR_MAX)
    {
        /* error reading clients file descriptor, that means we
         * cannot send error message to client as we don't know who
         * send it to us. Warning log is printed in
         * psmqd_broker_get_fd() so we simply exit
         */

        return -1;
    }

    err = 0;
    if (msg->paylen < 3)
    {
        el_oprint(OELW, "[%3d] subscribe error, topic '%s' too short %lu",
            fd, stopic, msg->paylen);
        err = EBADMSG;
    }
    else if (strlen(stopic) != msg->paylen - 1)
    {
        el_oprint(OELW,
            "[%3d] subscribe error, topic length %d "
            "doesn't match paylen (%lu - 1)",
            fd, strlen(stopic), msg->paylen);
        err = EBADMSG;
    }
    else if (stopic[0] != '/')
    {
        el_oprint(OELW, "[%3d] subscribe error, topic %s must start with '/'",
            fd, stopic);
        err = EBADMSG;
    }
    else if (stopic[msg->paylen - 2] == '/')
    {
        el_oprint(OELW, "[%3d] subscribe error, topic %s cannot end with '/'",
            fd, stopic);
        err = EBADMSG;
    }
    else
    {
        for (; *stopic != '\0'; ++stopic)
        {
            if (*stopic == '/' && *(stopic + 1) == '/')
            {
                el_oprint(OELW, "[%3d] subscribe error, topic '%s' cannot "
                        "contain two '/' in a row", fd, (char *)msg->payload);
                err = EBADMSG;
                break;
            }
        }
    }

    if (err)
    {
        psmqd_broker_reply(fd, PSMQ_TOPIC_SUBSCRIBE, &err, sizeof(err), 0);
        return -1;
    }


    stopic = (char *)msg->payload;
    if (psmqd_tl_add(&clients[fd].topics, stopic) != 0)
    {
        /* subscription failed
         */

        err = errno < UCHAR_MAX ? errno : UCHAR_MAX;
        el_operror(OELW, "[%3d] failed to add topic to list", fd);
        psmqd_broker_reply(fd, PSMQ_TOPIC_SUBSCRIBE, &err, sizeof(err), 0);
        return -1;
    }

    err = 0;
    psmqd_broker_reply(fd, PSMQ_TOPIC_SUBSCRIBE, &err, sizeof(err), 0);
    el_oprint(OELN, "[%3d] subscribed to %s", fd, stopic);
    return 0;
}


/* ==========================================================================
    Enables or disables client to receive messages. After that message
    messages matching client's subscribed topics will be sent to him or not
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    request:
            topic       str     "-E/NNN" where E is 'e' or 'd' and
                                    NNN is file descriptor
            payload     -       none

    response
            topic       str     "-E" where E is 'e' or 'd'
            payload     uchar   0 as in no errors
   ========================================================================== */


static int psmqd_broker_enable
(
    struct psmq_msg_pub  *msg, /* messages request */
    int                   en   /* enable or disable */
)
{
    unsigned char         fd;  /* client's file descriptor */
    unsigned char         err; /* error to send to client as reply */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    fd = psmqd_broker_get_fd(msg);
    if (fd == UCHAR_MAX)
    {
        return -1;
    }

    clients[fd].enabled = en;
    err = 0;
    psmqd_broker_reply(fd, en ? PSMQ_TOPIC_ENABLE : PSMQ_TOPIC_DISABLE,
        &err, sizeof(err), 0);
    el_oprint(OELN, "[%3d] client %s", fd, en ? "enabled" : "disabled");
    return 0;
}


/* ==========================================================================
    Unsubscribes client from specified topic
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    request:
            topic       str     "-u/NNN" where NNN is file descriptor
            payload     str     string with topic to unsubscribe

    response
            topic       str     "-u"
            payload     uchar   0 or errno value on failure

    errno:
            UCHAR_MAX   returned errno from system is bigger than UCHAR_MAX
   ========================================================================== */


static int psmqd_broker_unsubscribe
(
    struct psmq_msg_pub  *msg  /* messages request */
)
{
    unsigned char         fd;  /* client's file descriptor */
    unsigned char         err; /* error to send to client as reply */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    fd = psmqd_broker_get_fd(msg);
    if (fd == UCHAR_MAX)
    {
        return -1;
    }

    if (psmqd_tl_delete(&clients[fd].topics, (char *)msg->payload) != 0)
    {
        err = errno < UCHAR_MAX ? errno : UCHAR_MAX;
        el_operror(OELW, "[%3d] unsubscribe failed, topic: %s", fd,
            (char *)msg->payload);
        psmqd_broker_reply(fd, PSMQ_TOPIC_UNSUBSCRIBE, &err, sizeof(err), 0);
        return -1;
    }

    err = 0;
    el_oprint(OELN, "[%3d] unsubscribed %s", fd, (char *)msg->payload);
    psmqd_broker_reply(fd, PSMQ_TOPIC_UNSUBSCRIBE, &err, sizeof(err), 0);
    return 0;
}


/* ==========================================================================
    Closes connection with client and removes all resources alloceted by him
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    request:
            topic       str     "-c/NNN" where NNN is file descriptor
            payload     -       none

    response
            topic       str     "-c"
            payload     uchar   0 or errno value on failure
   ========================================================================== */


static int psmqd_broker_close
(
    struct psmq_msg_pub  *msg    /* messages request */
)
{
    unsigned char         fd;    /* client's file descriptor */
    unsigned char         err;   /* error to send to client as reply */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    fd = psmqd_broker_get_fd(msg);
    if (fd == UCHAR_MAX)
    {
        return -1;
    }

    /* send reply to broker we processed his close request,
     * yes we lie to him, but we won't be able to ack him after
     * we close communication queue
     */

    err = 0;
    psmqd_broker_reply(fd, PSMQ_TOPIC_CLOSE, &err, sizeof(err), 0);

    /* first delete all topics client is subscribed to
     */

    psmqd_tl_destroy(clients[fd].topics);

    /* then close mq and set it to -1 to indicate slot is free for
     * next client
     */

    mq_close(clients[fd].mq);
    clients[fd].mq = (mqd_t)-1;
    clients[fd].topics = NULL;
    el_oprint(OELN, "[%3d] closed, bye bye", fd);

    return 0;
}


/* ==========================================================================
    Process published message by one of the clients and send it to all
    interested parties.
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    request:
            topic       str     message topic
            payload     any     published data
            paylen      int     size of payload

    response:
            none        -       psmq doesn't use any id to identify messages
                                so OK response to publishing client is quite
                                pointless, as he wouldn't know which message
                                has been accepted. Also we don't know who
                                sent a message.
   ========================================================================== */


static int psmqd_broker_publish
(
    struct psmq_msg_pub  *msg,    /* published message by client */
    unsigned int          prio    /* message priority */
)
{
    unsigned char         fd;     /* client's file descriptor */
    struct psmqd_tl       *node;  /* node with subscribed topic */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    el_oprint(OELD, "received publish from topic %s, payload:", msg->topic);
    el_opmemory(OELD, msg->payload, msg->paylen);

    VALIDOP(E2BIG, strlen(msg->topic) <= sizeof(msg->topic));
    VALIDOP(E2BIG, msg->paylen <= sizeof(msg->payload));
    VALIDOP(ENOBUFS,
        strlen(msg->topic) < size_of_member(struct psmq_msg, topic));
    VALIDOP(ENOBUFS, msg->paylen <= size_of_member(struct psmq_msg, payload));


    /* iterate through all clients and send message to whoever
     * is subscribed and enabled
     */

    for (fd = 0; fd != PSMQ_MAX_CLIENTS; ++fd)
    {
        if (clients[fd].mq == (mqd_t)-1)
        {
            /* no client in this slot
             */

            continue;
        }

        if (clients[fd].enabled == 0)
        {
            /* client doesn't want to receive any published
             * messages
             */

            continue;
        }

        /* iterate through list of topics for that client to
         * check if he is interested in that message
         */

        for (node = clients[fd].topics; node != NULL; node = node->next)
        {
            if (psmqd_broker_topic_matches(msg->topic, node->topic) == 0)
            {
                /* nope, client is not subscribed to that topic
                 */

                continue;
            }

            /* yes, we have a match, send message to the client
             */

            if (psmqd_broker_reply(fd, msg->topic, msg->payload,
                    msg->paylen, prio) != 0)
            {
                el_operror(OELE, "[%3d] sending failed. topic %s, prio %u,"
                    " payload:", fd, msg->topic, prio);
                el_opmemory(OELE, msg->payload, msg->paylen);
                continue;
            }

            el_oprint(OELD, "publish %s to %d", msg->topic, fd);

            /* now it may be possible that another topic will match
             * for this client, for example when topic is /a/s/d
             * and client subscribed to /a/s/d and /a/s/+. So we
             * leave this loop, so we don't send him same message
             * twice
             */

            break;
        }
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
    Iniatializes broker, opens queues.
   ========================================================================== */


int psmqd_broker_init(void)
{
    struct mq_attr  mqa;  /* attributes for broker control queue */
    int             i;    /* iterator */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* invalidate all clients, to mark those slot as unused
     */

    for (i = 0; i != PSMQ_MAX_CLIENTS; ++i)
    {
        clients[i].mq = (mqd_t)-1;
    }

    if (g_psmqd_cfg.remove_queue)
    {
        /* remove control queue if it exist
         */

        if (mq_unlink(g_psmqd_cfg.broker_name) != 0)
        {
            if (errno != ENOENT)
            {
                /* error, other than queue does not exit (which is
                 * not an error in this case) occured. Most probably
                 * user has no rights to delete this queue.
                 */

                el_operror(OELF, "mq_unlink()");
            }
        }
    }

    /* open message queue for control, we will receive various
     * (like register, publish or subscribe) requests via it
     */

    mqa.mq_msgsize = sizeof(struct psmq_msg_pub);
    mqa.mq_maxmsg = g_psmqd_cfg.broker_maxmsg;
    qctrl = mq_open(g_psmqd_cfg.broker_name, O_RDWR | O_CREAT | O_EXCL,
        0600, &mqa);

    if (qctrl == (mqd_t)-1)
    {
        el_operror(OELF, "mq_open()");
        return -1;
    }

    el_oprint(OELN, "created queue %s with msgsize %d maxsize %d",
        g_psmqd_cfg.broker_name, mqa.mq_msgsize, mqa.mq_maxmsg);
    return 0;
}


/* ==========================================================================
    Main loop of the broker, it waits for messages and processes them.
    Received messages are initially validated here, so once message is
    rececived, function makes sure that topic is null terminated and
    paylen is not bigger than payload array.
   ========================================================================== */


int psmqd_broker_start(void)
{
    el_oprint(OELN, "starting psmqd broker main loop");

    for (;;)
    {
        struct timespec      tp;    /* timeout for mq_timedreceive() */
        struct psmq_msg_pub  msg;   /* received message from client */
        unsigned int         prio;  /* received message priority */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        clock_gettime(CLOCK_REALTIME, &tp);
        tp.tv_sec += 5;

        if (g_psmqd_shutdown)
        {
            /* shutdown flag was set, so we exit our loop, and
             * terminate
             */

            el_oprint(OELN, "shutdown flag was set, exit broker");
            return 0;
        }

        /* wait for message from client, there is no need to use
         * timedreceive, to check for g_psmqd_shutdown flag, as we will
         * get EINTR when signal is received
         *
         * Now there is very slim chance, that when SIGINT comes in
         * to set g_psmqd_shutdown to 1, it comes exactly in this
         * very place, that is after if() check and before
         * mq_receive() and that will cause deadlock, since program
         * will lock in mq_receive() after signal arrives, so it
         * will be locked forever (or until someone sends another
         * signal ot some message). To prevent it, we wake up every
         * 5 seconds to force another check of shutdown flag. It's
         * a small price to pay for not having deadlocks.
         */

        if (mq_timedreceive(qctrl, (char *)&msg, sizeof(msg), &prio, &tp) < 0)
        {
            if (errno == EINTR || errno == ETIMEDOUT)
            {
                /* interrupted by signal, continue to check if it
                 * was shutdown signal or not
                 *
                 * or
                 *
                 * timeout occured (check commend few lines up to
                 * see why it's here
                 */

                continue;
            }

            /* got some other, fatal, error. Log and exit since
             * error is unrecoverable
             */

            el_operror(OELF, "mq_receive(qctrl)");
            return -1;
        }

        /* message, received, now what to do with it? Well, at
         * first let's try to validate it
         */

        if (msg.paylen > sizeof(msg.payload))
        {
            /* message is bigger then buffer allows it, discard it
             */

            el_oprint(OELW, "got paylen %d when max is %d",
                msg.paylen, sizeof(msg.payload));
            continue;
        }

        /* all topics should be strings, so it is safe to nullify
         * last character of topic array, we do this just in case
         * when someone sends not null terminated topic
         */

        msg.topic[sizeof(msg.topic) - 1] = '\0';

        if (msg.topic[0] == '-')
        {
            /* all topics that start from '-' character are
             * reserved for control messages, so let's pass message
             * to control functions
             */

            el_oprint(OELD, "got control message '%c' payload '%s'",
                msg.topic[1],
                msg.paylen > 0 ? (const char *)msg.payload : "none");

            switch (msg.topic[1])
            {
            case 'o':
                /* open request, we expect string in payload, so
                 * it's safe to null last byte in payload array
                 */

                msg.payload[sizeof(msg.payload) - 1] = '\0';
                psmqd_broker_open((char *)msg.payload);
                break;

            case 'c':
                /* close request
                 */

                if (strlen(msg.topic) < 4 || msg.topic[2] != '/')
                {
                    /* control msg is in format "-c/N" where N
                     * is fd, so if topic is less than 4 bytes
                     * there is no way there is valid fd there
                     */

                    el_oprint(OELW, "invalid control topic %s", msg.topic);
                    continue;
                }

                psmqd_broker_close(&msg);
                break;

            case 's':
                /* subscribe requst,
                 */

                if (strlen(msg.topic) < 4 || msg.topic[2] != '/')
                {
                    el_oprint(OELW, "invalid control topic %s", msg.topic);
                    continue;
                }

                msg.payload[sizeof(msg.payload) - 1] = '\0';
                psmqd_broker_subscribe(&msg);
                break;

            case 'u':
                /* unsibscribe request
                 */

                if (strlen(msg.topic) < 4 || msg.topic[2] != '/')
                {
                    el_oprint(OELW, "invalid control topic %s", msg.topic);
                    continue;
                }

                msg.payload[sizeof(msg.payload) - 1] = '\0';
                psmqd_broker_unsubscribe(&msg);
                break;

            case 'e':
                /* enable request
                 */

                if (strlen(msg.topic) < 4 || msg.topic[2] != '/')
                {
                    el_oprint(OELW, "invalid control topic %s", msg.topic);
                    continue;
                }

                psmqd_broker_enable(&msg, 1);
                break;

            case 'd':
                /* disable request
                 */

                if (strlen(msg.topic) < 4 || msg.topic[2] != '/')
                {
                    el_oprint(OELW, "invalid control topic %s", msg.topic);
                    continue;
                }

                psmqd_broker_enable(&msg, 0);
                break;

            default:
                el_oprint(OELW, "received unknown request '%c'",
                    msg.topic[1]);
            }

            continue;
        }

        /* any other message is treated as data to be broadcastead
         * to subscribed clients
         */

        if (psmqd_broker_publish(&msg, prio) != 0)
        {
            el_operror(OELE, "psmqd_broker_publish() failed, topic %s, "
                "prio: %u, payload", msg.topic, prio);
            el_opmemory(OELE, msg.payload, msg.paylen);
        }
    }
}


/* ==========================================================================
    Closes all active connections and frees all allocated resources.
   ========================================================================== */


int psmqd_broker_cleanup(void)
{
    int  fd;  /* file descriptor */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* close all opened connections
     */

    for (fd = 0; fd != PSMQ_MAX_CLIENTS; ++fd)
    {
        struct psmq_msg_pub  msg;
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        if (clients[fd].mq == (mqd_t)-1)
        {
            /* empty slot, do nothing
             */

            continue;
        }

        /* trigger close request for the client, this will close
         * connection and free all resources, clients will be
         * informed about connection close.
         */

        sprintf(msg.topic, PSMQ_TOPIC_CLOSE"/%d", fd);
        msg.paylen = 0;
        psmqd_broker_close(&msg);
    }

    /* close control mqueue
     */

    mq_close(qctrl);
    mq_unlink(g_psmqd_cfg.broker_name);
    return 0;
}
