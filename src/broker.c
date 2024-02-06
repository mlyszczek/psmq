/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    uthor: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         -------------------------------------------------------------
        / Broker modules is reponsible for processing events from the \
        | clients. This is where clients are initialized. subscribed. |
        | and all published messages are propagaded to subscribed     |
        \ clients                                                     /
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
#include "psmq.h"


/* ==========================================================================
                  _                __           __
    ____   _____ (_)_   __ ____ _ / /_ ___     / /_ __  __ ____   ___   _____
   / __ \ / ___// /| | / // __ `// __// _ \   / __// / / // __ \ / _ \ / ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / /_ / /_/ // /_/ //  __/(__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/   \__/ \__, // .___/ \___//____/
/_/                                              /____//_/
   ========================================================================== */


/* structure describing connected client */
struct client
{
	/* descriptor of client's message queue, if this is (mqd_t)-1,
	 * then that means client is not connected */
	mqd_t  mq;

	/* list of topics client is subscribed to, if NULL, client is
	 * not subscribed to any topic */
	struct psmqd_tl  *topics;

	/* how many times did client missed pub because it's queue
	 * was full? */
	unsigned char  missed_pubs;

	/* how long will broker wait for client to free up space in
	 * its mqueue after giving up and discarding message */
	unsigned short  reply_timeout;
};


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


#define EL_OPTIONS_OBJECT &g_psmqd_log
#define PSMQ_MAX_MISSED_PUBS 10
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


static unsigned char psmqd_broker_get_free_client(void)
{
	int  fd;  /* number of free slot in clients */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	for (fd = 0; fd != PSMQ_MAX_CLIENTS; ++fd)
		if (clients[fd].mq == (mqd_t)-1)
			return fd; /* mq not set, slot available */

	/* all slots are used */
	return UCHAR_MAX;
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
	char        *pubts;      /* saveptr of pubt for strtok_r */
	char        *subts;      /* saveptr of subt for strtok_r */
	char        *pubtok;     /* current token of pub topic */
	char        *subtok;     /* current token of sub topic */
	char         pubt[PSMQ_MSG_MAX];  /* copy of pub_topic */
	char         subt[PSMQ_MSG_MAX];  /* copy of sub_topic */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	/* topics are equal, no need to parse them */
	if (strcmp(pub_topic, sub_topic) == 0)
		return 1;

	strcpy(pubt, pub_topic);
	strcpy(subt, sub_topic);

	/* first tokenize will always be valid, since
	 * all topics must start from '/' and this is
	 * checked before this function is called */

	pubtok = strtok_r(pubt, "/", &pubts);
	subtok = strtok_r(subt, "/", &subts);

	/* let's iterate through all parts of the topic */
	while(pubtok && subtok)
	{
		if (strcmp(subtok, "*") == 0)
		{
			/* path is equal up until now, and
			 * current token is '*' so we can stop
			 * parsing now and treat topics as
			 * matching.
			 *
			 * Note, '*' was replaces with '@' to
			 * make compilers happy.  A little
			 * quiz, guess why?
			 *
			 * Examples of matching topics:
			 *
			 *      pub: /a/b/c/d
			 *      sub: /a/b/@
			 *
			 *      pub: /a/b/c/d
			 *      sub: /a/@
			 *
			 *      pub: /a/b/c/d
			 *      sub: /@
			 */

			return 1;
		}

		if (strcmp(subtok, "+") == 0)
		{
			/* path is equal up intil now, and
			 * current token is '+' so we treat
			 * whole token as valid and continue
			 * for the next one.
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
			return 0; /* topics don't match */

		/* tokens are valid, take another token
		 * and continue with the checks */
		pubtok = strtok_r(NULL, "/", &pubts);
		subtok = strtok_r(NULL, "/", &subts);
	}

	/* we get here when we read all tokenes from
	 * either pub topic or sub topic */

	/* if both sub topic and pub topic were fully
	 * tokenized and they match, topics are equal */
	if (subtok == pubtok)
		return 1;

	/* only one of the sub topic or pub topic was
	 * tokenized fully, so these topics don't match */
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
	char             cmd,      /* command to which reply applies */
	unsigned char    data,     /* errno reply */
	const char      *topic,    /* topic to send message with */
	const void      *payload,  /* data to send to the client */
	unsigned         paylen,   /* length of payload to send */
	unsigned int     prio,     /* message priority */
	unsigned short   timeout   /* timeout in milliseconds */
)
{
	struct psmq_msg  msg;      /* structure with message to send */
	struct timespec  tp;       /* absolute time when mq_send() call expire */
	unsigned         topiclen; /* length of topic */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	memset(&msg, 0x00, sizeof(msg));
	msg.ctrl.cmd = cmd;
	msg.ctrl.data = data;
	msg.paylen = paylen;
	topiclen = 0;

	if (topic)
	{
		topiclen = strlen(topic) + 1;
		strcpy(msg.data, topic);
	}

	if (payload && paylen)
		memcpy(msg.data + topiclen, payload, paylen);

	/* send data to client, but do not wait if its queue
	 * is full, if it cannot process messages quick enough
	 * it does not deserve new message */
	psmq_ms_to_tp(timeout, &tp);
	return mq_timedsend(mq, (char *)&msg, psmq_real_msg_size(msg), prio, &tp);
}


/* ==========================================================================
    Same as psmqd_broker_reply_mq() but accepts fd instead of mqueue. Will
    also increment missed_pubs counter when message could not have been
    delivered to the client.
   ========================================================================== */


static int psmqd_broker_reply
(
	int           fd,       /* fd of client to send message to */
	char          cmd,      /* command to which reply applies */
	unsigned char data,     /* errno reply */
	const char   *topic,    /* topic to send message with */
	const void   *payload,  /* data to send to the client */
	unsigned      paylen,   /* length of payload to send */
	unsigned int  prio      /* message priority */
)
{
	if (psmqd_broker_reply_mq(clients[fd].mq, cmd,
			data,topic, payload, paylen, prio, clients[fd].reply_timeout) == 0)
	{
		clients[fd].missed_pubs = 0;
		return 0;
	}

	clients[fd].missed_pubs += 1;
	return -1;
}


/* ==========================================================================
    Same as psmqd_broker_reply_mq() but used to reply to control requests.
    These replies holds no data but control bytes.
   ========================================================================== */


static int psmqd_broker_reply_ctrl
(
	int           fd,       /* fd of client to send message to */
	char          cmd,      /* command to which reply applies */
	unsigned char data,     /* errno reply */
	const char   *extra     /* extra string data to send back to client */
)
{
	return psmqd_broker_reply(fd, cmd, data, extra, NULL, 0, 0);
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
            ctrl.cmd    char    PSMQ_CTRL_CMD_OPEN
            ctrl.data   uchar   ignored
            data        str     queue name where messages will be sent

    response:
            ctrl.cmd    char    PSMQ_CTRL_CMD_OPEN
            ctrl.data   uchar   0 on success, or errno
            data
                fd      uchar   file descriptor to use when communicating,
                                field is valid only when ctrl.data is 0

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
            laugh into my face for such ridiculous limitation.
   ========================================================================== */


static int psmqd_broker_open
(
	struct psmq_msg  *msg      /* msg with queue name to open */
)
{
	mqd_t             qc;      /* new communication queue */
	unsigned char     fd;      /* new file descriptor for the client */
	char             *qname;   /* queue name to open */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	/* qname validated for null
	 * termination during reception */
	qname = msg->data;

	/* open communication line with client */
	qc = mq_open(qname, O_RDWR);
	if (qc == (mqd_t)-1)
	{
		/* couldn't open queue provided by client and thus we have
		 * no way of informing him about error, so we only log this
		 * and abort this session */
		el_operror(OELC, "open failed: mq_open(%s)", qname);
		return -1;
	}

	fd = psmqd_broker_get_free_client();
	if (fd == UCHAR_MAX)
	{
		/* all slots are taken, send error information to the client */
		el_oprint(OELW, "open failed client %s: no free slots", qname);
		psmqd_broker_reply_mq(qc, PSMQ_CTRL_CMD_OPEN, ENOSPC,
				NULL, NULL, 0, 0, 0);
		mq_close(qc);
		return -1;
	}

	clients[fd].mq = qc;

	/* we have free slot and all data has been allocated, send
	 * client file descriptor he can use to control communication */
	if (psmqd_broker_reply_mq(qc, PSMQ_CTRL_CMD_OPEN,
				0, NULL, &fd, 1, 0, 0) == 0)
	{
		el_oprint(OELN, "[%3d] opened %s", fd, qname);
		return 0;
	}

	/* for some reason we couldn't send fd to client, log it
	 * and cleanup this mess */
	el_operror(OELW, "couldn't send fd to client %s", qname);
	mq_close(qc);
	clients[fd].mq = (mqd_t)-1;
	return -1;
}


/* ==========================================================================
    Subscribes to topic in message payload
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    request:
            ctrl.cmd    char    PSMQ_CTRL_CMD_SUBSCRIBE
            ctrl.data   uchar   file descriptor
            data        str     topic to subscribe to

    response
            ctrl.cmd    char    PSMQ_CTRL_CMD_SUBSCRIBE
            ctrl.data   uchar   0 on success, otherwise errno
            data        -       topic used tried to subscribe to

    errno for response:
            EBADMSG     payload is not a string or not a valid topic
            UCHAR_MAX   returned errno from system is bigger than UCHAR_MAX
   ========================================================================== */


static int psmqd_broker_subscribe
(
	struct psmq_msg  *msg         /* subscription request */
)
{
	unsigned char     fd;         /* clients file descriptor */
	unsigned char     err;        /* errno value to send to client */
	char             *stopic;     /* subscribe topic from client */
	char             *stopicsave; /* saved pointer of stopic */
	unsigned          stopiclen;  /* length of stopic */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	fd = msg->ctrl.data;
	stopic = msg->data;
	stopiclen = strlen(stopic);
	err = 0;

	if (stopiclen < 2)
	{
		el_oprint(OELW, "[%3d] subscribe error, topic '%s' too short %u",
				fd, stopic, stopiclen);
		err = EBADMSG;
	}
	else if (msg->paylen != 0)
	{
		el_oprint(OELW, "[%3d] subscribe error, message contains extra data", fd);
		err = EBADMSG;
	}
	else if (stopic[0] != '/')
	{
		el_oprint(OELW, "[%3d] subscribe error, topic %s must start with '/'",
				fd, stopic);
		err = EBADMSG;
	}
	else if (stopic[stopiclen - 1] == '/')
	{
		el_oprint(OELW, "[%3d] subscribe error, topic %s cannot end with '/'",
				fd, stopic);
		err = EBADMSG;
	}
	else
	{
		for (stopicsave = stopic; *stopic != '\0'; ++stopic)
		{
			if (*stopic == '/' && *(stopic + 1) == '/')
			{
				el_oprint(OELW, "[%3d] subscribe error, topic '%s' cannot "
						"contain two '/' in a row", fd, stopicsave);
				err = EBADMSG;
				break;
			}
		}

		stopic = stopicsave;
	}

	if (err)
	{
		psmqd_broker_reply_ctrl(fd, PSMQ_CTRL_CMD_SUBSCRIBE, err, stopic);
		return -1;
	}


	if (psmqd_tl_add(&clients[fd].topics, stopic) != 0)
	{
		/* subscription failed */
		err = errno < UCHAR_MAX ? errno : UCHAR_MAX;
		el_operror(OELW, "[%3d] failed to add topic to list", fd);
		psmqd_broker_reply_ctrl(fd, PSMQ_CTRL_CMD_SUBSCRIBE, err, stopic);
		return -1;
	}

	psmqd_broker_reply_ctrl(fd, PSMQ_CTRL_CMD_SUBSCRIBE, 0, stopic);
	el_oprint(OELN, "[%3d] subscribed to %s", fd, stopic);
	return 0;
}


/* ==========================================================================
    Unsubscribes client from specified topic
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    request:
            ctrl.cmd    char    PSMQ_CTRL_CMD_UNSUBSCRIBE
            ctrl.data   uchar   file descriptor of the client
            data        str     string with topic to unsubscribe

    response
            ctrl.cmd    char    PSMQ_CTRL_CMD_UNSUBSCRIBE
            ctrl.data   uchar   0 on success, otherwise errno
            data        -       none

    errno:
            UCHAR_MAX   returned errno from system is bigger than UCHAR_MAX
   ========================================================================== */


static int psmqd_broker_unsubscribe
(
	struct psmq_msg  *msg     /* messages request */
)
{
	unsigned char     fd;     /* client's file descriptor */
	unsigned char     err;    /* error to send to client as reply */
	char             *utopic; /* topic to unsubscribe */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	fd = msg->ctrl.data;
	utopic = msg->data;

	if (psmqd_tl_delete(&clients[fd].topics, utopic) != 0)
	{
		err = errno < UCHAR_MAX ? errno : UCHAR_MAX;
		el_operror(OELW, "[%3d] unsubscribe failed, topic: %s", fd, utopic);
		psmqd_broker_reply_ctrl(fd, PSMQ_CTRL_CMD_UNSUBSCRIBE, err, utopic);
		return -1;
	}

	el_oprint(OELN, "[%3d] unsubscribed %s", fd, utopic);
	psmqd_broker_reply_ctrl(fd, PSMQ_CTRL_CMD_UNSUBSCRIBE, 0, utopic);
	return 0;
}


/* ==========================================================================
    Closes connection with client and removes all resources alloceted by him
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    request:
            ctrl.cmd    char    PSMQ_CTRL_CMD_CLOSE
            ctrl.data   uchar   file descriptor of the client
            data        -       none

    response
            ctrl.cmd    char    PSMQ_CTRL_CMD_CLOSE
            ctrl.data   uchar   0 on success, otherwise errno
            data        -       none
   ========================================================================== */


static int psmqd_broker_close
(
	int  fd  /* client's file descriptor */
)
{
	/* first, zero out missed_pubs or else we risk
	 * infinite recursive loop when sending close
	 * command to client triggers close function
	 * over and over again until all that is right
	 * and good is dead */
	clients[fd].missed_pubs = 0;

	/* send reply to broker we processed his close
	 * request, yes we lie to him, but we won't be
	 * able to ack him after we close communication
	 * queue */
	psmqd_broker_reply_ctrl(fd, PSMQ_CTRL_CMD_CLOSE, 0, NULL);

	/* first delete all topics client is subscribed to */
	psmqd_tl_destroy(clients[fd].topics);

	/* then close mq and set it to -1 to indicate
	 * slot is free for next client */
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
            ctrl.cmd    char    PSMQ_CTRL_CMD_PUBLISH
            ctrl.data   uchar   file descriptor of requesting client
            paylen      uint    size of data.payload
            data
                topic   str     topic to publish message on
                payload any     data to publish

    response:
            none        -       psmq doesn't use any id to identify messages
                                so OK response to publishing client is quite
                                pointless, as he wouldn't know which message
                                has been accepted.
   ========================================================================== */


static int psmqd_broker_publish
(
	struct psmq_msg  *msg,       /* published message by client */
	unsigned int      prio       /* message priority */
)
{
	unsigned char     fd;        /* client's file descriptor */
	struct psmqd_tl   *node;     /* node with subscribed topic */
	void              *payload;  /* payload to publish */
	char              *topic;    /* topic to publish message on */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	/* topic is verified for null termination
	 * during data reception */
	topic = msg->data;
	payload = msg->data + strlen(topic) + 1;

	el_oprint(OELD, "received publish from topic %s, payload (len: %u):",
			topic, msg->paylen);
	el_opmemory(OELD, payload, msg->paylen);

	/* iterate through all clients and send
	 * message to whoever is subscribed */
	for (fd = 0; fd != PSMQ_MAX_CLIENTS; ++fd)
	{
		/* do we have client in this slot? */
		if (clients[fd].mq == (mqd_t)-1)
			continue;  /* nope */

		/* iterate through list of topics for that
		 * client to check if he is interested in
		 * that message */
		for (node = clients[fd].topics; node != NULL; node = node->next)
		{
			/* is client subscribed to current topic? */
			if (psmqd_broker_topic_matches(topic, node->topic) == 0)
				continue;  /* nope */

			/* yes, we have a match, send message to the client */
			if (psmqd_broker_reply(fd, PSMQ_CTRL_CMD_PUBLISH, 0,
						topic, payload, msg->paylen, prio) != 0)
			{
				el_operror(OELE, "[%3d] sending failed. topic %s, prio %u,"
						" payload (len: %u):", fd, topic, prio, msg->paylen);
				el_opmemory(OELE, payload, msg->paylen);

				if (clients[fd].missed_pubs >= PSMQ_MAX_MISSED_PUBS)
				{
					struct psmq_msg  dummy;
					struct timespec  tp;
					/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


					el_oprint(OELE, "[%3d] failed to send msg to client "
							"for %d consecutive calls, delete client",
							fd, PSMQ_MAX_MISSED_PUBS);

					/* client assumed dead, it's queue now is
					 * most probably full, but there still is
					 * a chance that client is alive but just
					 * hanged for a long time and will eventualy
					 * read something from queue, but since
					 * queue is full we cannot send him close
					 * message. So to make sure there is close
					 * message on the queue, we remove oldest
					 * message from it and then call close().
					 * We do not check for return code here,
					 * if it does not work there is nothing
					 * we can do. */
					tp.tv_sec = 0;
					tp.tv_nsec = 0;
					mq_timedreceive(clients[fd].mq, (char *)&dummy,
							sizeof(dummy), NULL, &tp);
					psmqd_broker_close(fd);
					break;
				}
				continue;
			}

			el_oprint(OELD, "published %s to %d", topic, fd);

			/* now it may be possible that another topic will match
			 * for this client, for example when topic is /a/s/d
			 * and client subscribed to /a/s/d and /a/s/+. So we
			 * leave this loop, so we don't send him same message
			 * twice */
			break;
		}
	}

	return 0;
}


/* ==========================================================================
    Changes settings for client to alter how broker interacts with client.
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

     request:
            ctrl.cmd    char    PSMQ_CTRL_CMD_IOCTL
            ctrl.data   uchar   file descriptor of requesting client
            paylen      uint    size of data.payload
            data
                ioctl   uchar   ioctl request
                data    varial  variable, depending on ioctl request

    response
            ctrl.cmd    char    PSMQ_CTRL_CMD_IOCTL
            ctrl.data   uchar   0 on success, otherwise errno
            paylen      uint    size of data.payload
            data
                ioctl   uchar   ioctl request that was requested to be set
                data    varial  newly set value of request
   ========================================================================== */


static void psmqd_broker_reply_ioctl
(
	int            fd,       /* fd of client to send message to */
	unsigned char  err,      /* errno reply */
	unsigned char  req,      /* ioctl request */
	void          *data,     /* extra data */
	int            datalen   /* length of extra */
)
{
	char           buf[16];  /* req + data to send to client */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	buf[0] = req;
	if (data)
		memcpy(buf + 1, data, datalen);
	psmqd_broker_reply(fd, PSMQ_CTRL_CMD_IOCTL, err, NULL, buf, 1 + datalen, 0);
}

static int psmqd_broker_ioctl
(
	struct psmq_msg  *msg       /* ioctl request */
)
{
	struct client    *client;   /* handle to client that requested ioctl */
	unsigned char     fd;       /* client's file descriptor */
	unsigned char     req;      /* ioctl request */
	char             *data;     /* data associated with req */
	unsigned short    dlen;     /* length of data */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	fd = msg->ctrl.data;
	req = msg->data[0];
	client = &clients[fd];

	if (msg->paylen < 1)
	{
		el_oprint(OELE, "[%3d] ioctl error, no request specified", fd);
		psmqd_broker_reply_ioctl(fd, EINVAL, 0, NULL, 0);
		return -1;
	}

	/* set data to contain only data associated
	 * with req without request id itself */
	dlen = msg->paylen - 1;
	data = msg->data + 1;

	switch (req)
	{
	case PSMQ_IOCTL_REPLY_TIMEOUT:
		if (dlen != sizeof(client->reply_timeout))
		{
			el_oprint(OELE, "[%3d] ioctl error, invalid data", fd);
			psmqd_broker_reply_ioctl(fd, EINVAL, req, data, dlen);
			return -1;
		}

		memcpy(&client->reply_timeout, data, dlen);
		el_oprint(OELN, "[%3d] ioctl: set reply_timeout to %u",
				fd, client->reply_timeout);
		psmqd_broker_reply_ioctl(fd, 0, req, &client->reply_timeout, dlen);
		return 0;

	default:
		el_oprint(OELE, "[%3d] ioctl error, invalid request: %d", fd, req);
		psmqd_broker_reply_ioctl(fd, EINVAL, req, NULL, 0);
		return -1;

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
    Iniatializes broker, opens queues.
   ========================================================================== */


int psmqd_broker_init(void)
{
	struct mq_attr  mqa;  /* attributes for broker control queue */
	int             i;    /* iterator */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	/* invalidate all clients, to mark those slot as unused */
	for (i = 0; i != PSMQ_MAX_CLIENTS; ++i)
		clients[i].mq = (mqd_t)-1;

	/* remove control queue if it exist
	 *
	 * ENOENT error (which translates to queue
	 * does not exist) is ignored as it's not
	 * really an error in this case. Most likely
	 * running user has no rights to delete this
	 * queue, but he still might be able to use it. */
	if (g_psmqd_cfg.remove_queue)
		if (mq_unlink(g_psmqd_cfg.broker_name) != 0)
			if (errno != ENOENT)
				el_operror(OELF, "mq_unlink()");

	/* open message queue for control, we will
	 * receive various (like register, publish or
	 * subscribe) requests via it */
	memset(&mqa, 0x00, sizeof(mqa));
	mqa.mq_msgsize = sizeof(struct psmq_msg);
	mqa.mq_maxmsg = g_psmqd_cfg.broker_maxmsg;

	/* now this is strage behaviour, on dragonfly bsd,
	 * when you call mq_open() fast enough after
	 * mq_unlink() it appears that this specific mqueue
	 * is still in the OS state, and mq_open() will
	 * return error, so let's try to open it couple
	 * of times before calling error. Maybe there
	 * are other OSes with behaviour like that? */
	for (i = 1; i <= 10; ++i)
	{
		qctrl = mq_open(g_psmqd_cfg.broker_name,
				O_RDWR | O_CREAT | O_EXCL, 0600, &mqa);

		if (qctrl != (mqd_t)-1)
			break;

		el_operror(OELF, "mq_open(); try %d/10", i);
	}

	/* tried to open mqueue for 10 times now, and
	 * it's still failing. Oh well. */
	if (i == 11)
		return -1;

	el_oprint(OELN, "created queue %s with msgsize %ld maxsize %ld",
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
		struct timespec  tp;        /* timeout for mq_timedreceive() */
		struct psmq_msg  msg;       /* received message from client */
		unsigned int     prio;      /* received message priority */
		unsigned         topiclen;  /* length of topic in msg.data */
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


		clock_gettime(CLOCK_REALTIME, &tp);
		tp.tv_sec += 5;

		if (g_psmqd_shutdown)
		{
			/* shutdown flag was set, so we exit our loop, and
			 * terminate */
			el_oprint(OELN, "shutdown flag was set, exit broker");
			return 0;
		}

		memset(&msg, 0x00, sizeof(msg));

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
		 * signal or some message). To prevent it, we wake up every
		 * 5 seconds to force another check of shutdown flag. It's
		 * a small price to pay for not having deadlocks. */
		if (mq_timedreceive(qctrl, (char *)&msg, sizeof(msg), &prio, &tp) < 0)
		{
			/* if we are interrupted by signal,
			 * continue to check if it was
			 * shutdown signal or not
			 *
			 * or
			 *
			 * if timeout occured continue to
			 * check for shutdown flag (check
			 * comment few lines up to see why
			 * it's here */
			if (errno == EINTR || errno == ETIMEDOUT)
				continue;

			/* got some other, fatal, error. Log
			 * and exit since error is * unrecoverable */
			el_operror(OELF, "mq_receive(qctrl)");
			return -1;
		}

		/* message, received, now what to do with
		 * it? Well, at first let's try to validate it */

		/* all topics must be strings, so check if it
		 * is nullified */
		topiclen = strlen(msg.data);
		if (topiclen >= sizeof(msg.data))
		{
			el_oprint(OELW, "incoming msg: topic is not null terminated "
					"hexdump of msg is:");
			el_opmemory(OELW, &msg, sizeof(msg));
			continue;
		}

		/* does message holds valid payload? */
		if (topiclen + 1 + msg.paylen > sizeof(msg.data))
		{
			/* topic + length of payload claimed by the
			 * client could not have fit into buffer. */
			el_oprint(OELW, "incoming msg: invalid paylen, hexdump of msg is:");
			el_opmemory(OELW, &msg, sizeof(msg));
			continue;
		}

		if (msg.ctrl.cmd != PSMQ_CTRL_CMD_OPEN &&
				msg.ctrl.data >= PSMQ_MAX_CLIENTS)
		{
			/* all messages are required to send valid
			 * file descriptor, only open request does
			 * not require it (since user requests fd
			 * to be allocated for him to use).
			 *
			 * We cannot send back error to the client
			 * since we do not have proper fd, so only
			 * log the warning. */
			el_oprint(OELW, "msg with invalid fd (%d) received, hexdump is:",
					msg.ctrl.data);
			el_opmemory(OELW, &msg, sizeof(msg));
			continue;
		}

		/* at this point we are sure that topic is properly
		 * nullified and payload fits into buffer */

		el_oprint(OELD, "got control message: %c", msg.ctrl.cmd);
		el_opmemory(OELD, &msg, psmq_real_msg_size(msg));

		switch (msg.ctrl.cmd)
		{
			case 'o': psmqd_broker_open(&msg); break;
			case 'c': psmqd_broker_close(msg.ctrl.data); break;
			case 's': psmqd_broker_subscribe(&msg); break;
			case 'u': psmqd_broker_unsubscribe(&msg); break;
			case 'p': psmqd_broker_publish(&msg, prio); break;
			case 'i': psmqd_broker_ioctl(&msg); break;
			default:
				el_oprint(OELW, "received unknown request '%c'",
						msg.data[1]);
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


	/* close all opened connections */
	for (fd = 0; fd != PSMQ_MAX_CLIENTS; ++fd)
	{
		/* for empty slots, do nothing */
		if (clients[fd].mq == (mqd_t)-1)
			continue;

		/* trigger close request for the client,
		 * this will close connection and free all
		 * resources, clients will be informed
		 * about connection close. */
		psmqd_broker_close(fd);
	}

	/* close control mqueue */
	mq_close(qctrl);
	mq_unlink(g_psmqd_cfg.broker_name);
	return 0;
}
