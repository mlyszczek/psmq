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
   ========================================================================== */


static int psmq_ack
(
	char            *topic,     /* topic of received ack */
	void            *payload,   /* payload of ack response */
	size_t           paylen,    /* length of payload buffer */
	unsigned int     prio,      /* not used */
	void            *userdata   /* not used */
)
{
	struct psmq_msg *msg;       /* message received from broker */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	(void)topic;
	(void)payload;
	(void)prio;

	msg = userdata;

	/* all ack messages have empty msg->data[] part, that is
	 * paylen must be 0, and first byte must be set to '\0',
	 * otherwise we treat such message as regular publish.
	 * Note that, regular publish can have paylen set to 0
	 * (no data) but topic must always be set, so msg->data[]
	 * will never be NULL for regular publish messages */
	if (paylen != 0 || msg->data[0] != '\0')
		return -2;

	return msg->ctrl.data;
}


/* ==========================================================================
    This is same as psmq_receive() but does not check for psmq->enabled flag
    Also, when full_msg is set to 1, pointer to msg is stored in userdata,
    useless and not used for lib clients, but usefull and surely used for
    internal ack reception. Note in that case user data will be overwritten!
   ========================================================================== */


static int psmq_receive_internal
(
	struct psmq     *psmq,     /* psmq object */
	psmq_sub_clbk    clbk,     /* function to be called with received data */
	void            *userdata, /* user data passed to clkb */
	int              full_msg  /* store msg in userdata */
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
		return -1;

	if (full_msg)
		userdata = &msg;

	return clbk(msg.data, msg.data + strlen(msg.data) + 1,
			msg.paylen, prio, userdata);
}


/* ==========================================================================
    Same as psmq_publish, but also accepts psmq_msg.ctrl part of message, to
    be able to send custom commands. Usefull only as internal usage.
    Exported externally because tests use this function, but its usage won't
    be documented and it is not guaranteed to have stable API/ABI.
   ========================================================================== */


int psmq_publish_msg
(
	struct psmq     *psmq,     /* psmq object */
	char             cmd,      /* message command */
	unsigned char    data,     /* data for the control part of message */
	const char      *topic,    /* topic of message to be sent */
	const void      *payload,  /* payload of message to be sent */
	size_t           paylen,   /* length of payload buffer */
	unsigned int     prio      /* message priority */
)
{
	struct psmq_msg  pub;      /* buffer used to send out data to broker */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	VALID(EINVAL, psmq);
	VALID(EBADF, psmq->qpub != (mqd_t)-1);
	VALID(EBADF, psmq->qsub != psmq->qpub);

	pub.ctrl.cmd = cmd;
	pub.ctrl.data = data;
	pub.paylen = 0;
	pub.data[0] = '\0';

	if (topic)
		strcpy(pub.data, topic);

	/* payload may be NULL and it's ok, so set
	 * payload only if it was set */
	if (payload)
	{
		memcpy(pub.data + (topic ? strlen(topic) + 1 : 0), payload, paylen);
		pub.paylen = paylen;
	}

	return mq_send(psmq->qpub, (char *)&pub, psmq_real_msg_size(pub), prio);
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
	int           ack;      /* response from the broker */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	VALID(EINVAL, psmq);
	VALID(EINVAL, topic);
	VALID(EINVAL, topic[0] != '\0');
	VALID(EBADF, psmq->qsub != (mqd_t)-1);
	VALID(EBADF, psmq->qsub != psmq->qpub);
	VALID(ENOBUFS, strlen(topic) + 1 <= PSMQ_MSG_MAX);

	/* send subscribe request to the server */
	if (psmq_publish_msg(psmq,
			sub ? PSMQ_CTRL_CMD_SUBSCRIBE : PSMQ_CTRL_CMD_UNSUBSCRIBE,
			psmq->fd, topic, NULL, 0, 0) != 0)
		return -1;

	/* if psmq is enabled (that means it is primed to receive
	 * normal messages), we cannot check for subscribe ack,
	 * as we could read normal message addressed to main
	 * application. In this mode user is responsible for
	 * reading ack - we return success here. */
	if (psmq->enabled)
		return 0;

	/* now wait for ack reply */
	ack = psmq_receive_internal(psmq, psmq_ack, NULL, 1);

	if (ack != 0)
	{
		if (ack > 0)
			errno = ack;
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
    priority to broker defined in 'psmq'. This is used only to publish
    real (non-control) messages.

    Returns 0 on success or -1 on errors

    errno:
            EINVAL      psmq is invalid (null)
            EBADF       psmq was not properly initialized
            ENOBUFS     topic or payload is to big to fit into buffers
   ========================================================================== */


int psmq_publish
(
	struct psmq     *psmq,     /* psmq object */
	const char      *topic,    /* topic of message to be sent */
	const void      *payload,  /* payload of message to be sent */
	size_t           paylen,   /* length of payload buffer */
	unsigned int     prio      /* message priority */
)
{
	struct psmq_msg  pub;      /* buffer used to send out data to broker */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	VALID(EINVAL, psmq);
	VALID(EINVAL, topic);
	VALID(ENOBUFS, strlen(topic) + 1 + paylen <= sizeof(pub.data));
	VALID(EBADMSG, topic[0] == '/');

	return psmq_publish_msg(psmq, PSMQ_CTRL_CMD_PUBLISH, psmq->fd,
			topic, payload, paylen, prio);
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
	return psmq_receive_internal(psmq, clbk, userdata, 0);
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
		return -1;

	return clbk(msg.data, msg.data + strlen(msg.data) + 1,
			msg.paylen, prio, userdata);
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


	/* no VALID() checks, since all parameters are
	 * validated in psmqd_timedreceive() function */

	tp.tv_sec = 0;
	tp.tv_nsec = 0;

	if (ms)
	{
		size_t  nsec;  /* number of nanoseconds to add to timer */
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


		/* milisecond is set, configure tp to wait
		 * this ammount of time instead of
		 * returning immediately */

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
	VALID(ENAMETOOLONG, mqnamelen < PSMQ_MSG_MAX);


	memset(psmq, 0x00, sizeof(struct psmq));
	mqa.mq_msgsize = sizeof(struct psmq_msg);
	mqa.mq_maxmsg = maxmsg;
	psmq->qsub = (mqd_t)-1;
	psmq->qpub = (mqd_t)-1;

	/* if staled queue exist, remove it, so it
	 * doesn't do weird stuff */
	mq_unlink(mqname);

	/* open subscribe queue, where we will receive
	 * data we subscribed to, and at the beginning
	 * is used to report error from broker to client */
	psmq->qsub = mq_open(mqname, O_RDONLY | O_CREAT, 0600, &mqa);
	if (psmq->qsub == (mqd_t)-1)
		return -1;

	/* open publish queue, this will be used to
	 * subscribe to topics at the start, and
	 * later this will be used to publish data on
	 * given topic */
	psmq->qpub = mq_open(brokername, O_WRONLY);
	if (psmq->qpub == (mqd_t)-1)
	{
		mq_close(psmq->qsub);
		psmq->qsub = (mqd_t)-1;
		return -1;
	}

	/* both queues have been created, that means
	 * we have enough memory to operate, now we
	 * need to register to broker, so it knows
	 * where to send messages */
	if (psmq_publish_msg(psmq, PSMQ_CTRL_CMD_OPEN, 0, mqname, NULL, 0, 0) != 0)
		goto error;

	/* check response from the broker on subscribe
	 * queue to check if broker managed to
	 * allocate memory for us and open queue on
	 * his side. Read from broker until we read
	 * open reply */
	for (;;)
	{
		struct psmq_msg  msg;  /* received psmq message */
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


		if ((ack = mq_receive(psmq->qsub, (char *)&msg, sizeof(msg), 0)) == -1)
			break;

		/* open response is suppose to send back
		 * message with open command, if that is
		 * not the case, this could mean that
		 * there may be some old data in queue.
		 * This can happend if we open queue from
		 * previously crashed session. Open
		 * response is first message we ever
		 * receive, so if there is anything else,
		 * these messages do not belong to us and
		 * we can safely discard them. */
		if (msg.ctrl.cmd != PSMQ_CTRL_CMD_OPEN)
			continue;

		/* no space left for us on the broker */
		ack = msg.ctrl.data;
		if (msg.paylen == 0 && ack == ENOSPC)
			break;

		if (msg.paylen != 1)
		{
			/* we expected exactly 1 byte as fd,
			 * anything else is wrong */
			ack = EBADMSG;
			break;
		}

		ack = msg.ctrl.data;
		psmq->fd = msg.data[0];
		break;
	}

	/* broker will return either 0 or errno to
	 * indicate what went wrong on his side, if
	 * ack is 0, broker will send file descriptor
	 * to use when communicating with him. */
	if (ack != 0)
	{
		if (ack > 0)
			errno = ack;

		goto error;
	}

	/* ack received and fd is valid,
	 * we are victorious */
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
	VALID(EINVAL, psmq);
	VALID(EBADF, psmq->qsub != psmq->qpub);

	/* send close() to the broker, we don't care
	 * if it succed or not, we close our booth and
	 * nothing can stop us from doing it */
	psmq_publish_msg(psmq, PSMQ_CTRL_CMD_CLOSE, psmq->fd, NULL, NULL, 0, 0);
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
	int           ack;    /* response from the broker */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	VALID(EINVAL, psmq);
	VALID(EBADF, psmq->qsub != (mqd_t)-1);
	VALID(EBADF, psmq->qsub != psmq->qpub);
	VALID(EINVAL, (enable & ~1) == 0);

	/* send enable/disable request to the server */
	if (psmq_publish_msg(psmq,
				enable ? PSMQ_CTRL_CMD_ENABLE : PSMQ_CTRL_CMD_DISABLE,
				psmq->fd, NULL, NULL, 0, 0) != 0)
		return -1;

	/* now wait for ack reply */
	ack = psmq_receive_internal(psmq, psmq_ack, NULL, 1);

	if (ack != 0)
	{
		if (ack > 0)
			errno = ack;

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

    Basically, it just sends disable request to the broker, but does not
    wait for confirmation.
   ========================================================================== */


int psmq_disable_threaded
(
	struct psmq  *psmq   /* psmq object */
)
{
	VALID(EINVAL, psmq);
	VALID(EBADF, psmq->qsub != (mqd_t)-1);
	VALID(EBADF, psmq->qsub != psmq->qpub);

	/* send disable request to the server */
	if (psmq_publish_msg(psmq, PSMQ_CTRL_CMD_DISABLE, psmq->fd,
				NULL, NULL, 0, 0) != 0)
		return -1;

	/* threaded disable, caller is reponsible of
	 * setting psmq->enabled to 0 */
	return 0;
}
