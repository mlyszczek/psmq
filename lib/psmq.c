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
#include <stdarg.h>
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
            EINVAL      topic is invalid (null)
            EBADF       psmq was not properly initialized
            EBADMSG     topic does not start from '/' character
            ENOBUFS     topic and/or payload are to big to fit into buffers
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
    Waits for message to be received from broker. Message is stored in msg
    buffer provided by caller. Function will receive both subscribed message
    as well as control messages (like subscribe confirmation). Function waits
    for message forever or until it is interrupted by signal.

    Returns 0 on success or -1 on errors

    errno:
            EINVAL      psmq is invalid (null)
            EBADF       psmq was not properly initialized
            EINTR       The call was interrupted by a signal handler
            EAGAIN      The queue was empty, and the O_NONBLOCK flag was set
                        for the message queue
   ========================================================================== */


int psmq_receive
(
	struct psmq      *psmq,  /* psmq object */
	struct psmq_msg  *msg,   /* received message */
	unsigned int     *prio   /* message priority */
)
{
	VALID(EINVAL, psmq);
	VALID(EINVAL, msg);
	VALID(EBADF, psmq->qpub != (mqd_t)-1);
	VALID(EBADF, psmq->qsub != psmq->qpub);

	/* return -1 on error otherwise return 0 */
	return -(mq_receive(psmq->qsub, (char *)msg, sizeof(*msg), prio) == -1);
}


/* ==========================================================================
    Same as psmq_receive(), but blocks only for ammount of time specified
    by absolute timespec 'tp', unlike psmq_receive() which will block until
    message is received (or is interrupted by signal).

    When timeout occurs, msg is not modified.
    When tp is 0, function returns immediately.

    Returns 0 on success or -1 on errors

    errno:
            EINVAL      psmq is invalid (null)
            EINVAL      msg is invalid (null)
            EINVAL      tp is invalid (null)
            EINVAL      tp is invalid (tv_sec less than zero, or
                        tv_nsec less than zero or grater than 1000 mil)
            EBADF       psmq was not properly initialized
            EINTR       The call was interrupted by a signal handler
            ETIMEDOUT   call timed out before message could be received
            EAGAIN      The queue was empty, and the O_NONBLOCK flag was set
                        for the message queue

    notes:
            On qnx (up until 6.4.0 at least) when timeout occurs,
            mq_timedreceveice will return EINTR instead of ETIMEDOUT
   ========================================================================== */


int psmq_timedreceive
(
	struct psmq      *psmq,  /* psmq object */
	struct psmq_msg  *msg,   /* received message */
	unsigned int     *prio,  /* message priority */
	struct timespec  *tp     /* absolute time to wait for timeout */
)
{
	VALID(EINVAL, psmq);
	VALID(EINVAL, msg);
	VALID(EINVAL, tp);
	VALID(EBADF, psmq->qpub != (mqd_t)-1);
	VALID(EBADF, psmq->qsub != psmq->qpub);

	return -(mq_timedreceive(psmq->qsub, (char *)msg,
				sizeof(*msg), prio, tp) == -1);
}


/* ==========================================================================
    Same as psmq_timedreceive() but accepts 'milisecond' time that shall
    pass before timeout should occur instead of absolute timespec.

    When timeout occurs, msg is not modified.
    When ms is 0, function returns immediately.

    Returns 0 on success or -1 on errors

    errno:
            EINVAL      psmq is invalid (null)
            EINVAL      msg is invalid (null)
            EBADF       psmq was not properly initialized
            EINTR       The call was interrupted by a signal handler
            ETIMEDOUT   call timed out before message could be received
            EAGAIN      The queue was empty, and the O_NONBLOCK flag was set
                        for the message queue

    notes:
            On qnx (up until 6.4.0 at least) when timeout occurs,
            mq_timedreceveice will return EINTR instead of ETIMEDOUT
   ========================================================================== */


int psmq_timedreceive_ms
(
	struct psmq      *psmq,  /* psmq object */
	struct psmq_msg  *msg,   /* received message */
	unsigned int     *prio,  /* message priority */
	size_t            ms     /* ms to wait until timeout occurs */
)
{
	struct timespec   tp;    /* absolute time to wait for timeout */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	VALID(EINVAL, psmq);
	VALID(EINVAL, msg);
	VALID(EBADF, psmq->qpub != (mqd_t)-1);
	VALID(EBADF, psmq->qsub != psmq->qpub);

	psmq_ms_to_tp(ms, &tp);
	return -(mq_timedreceive(psmq->qsub, (char *)msg,
				sizeof(*msg), prio, &tp) == -1);
}


/* ==========================================================================
    Create mqueue for receiving and opens connection to broker

    Return 0 when broker sends back connection confirmation or -1 when error
    occured.

    errno:
            EINVAL      psmq is invalid (null)
            EINVAL      brokername is invalid (null)
            EINVAL      brokername does not start with '/'
            EINVAL      mqname is invalid (null)
            EINVAL      mqname does not start with '/'
            EINVAL      maxmsg is 0 or less
            ENAMETOOLONG  mqname is bigger than PSMQ_MSG_MAX and thus cannot
                        be send to broker
            EACCES      Either brokername or mqname can't be opened due to
                        permissions
            EACCES      mqname or brokername contains more than one '/'
            ENFILE      system-wide limit on opened files has been reached
            EMFILE      per-process limit on opened files has been reached
            ENOENT      brokername does not exist
            ENOENT      mqname or brokername was just "/" and nothing else
            ENOMEM      not enough memory in the system
            ENOSPC      not enough space for the creation of a new queue
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


		if ((ack = psmq_receive(psmq, &msg, NULL)) == -1)
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
	mq_unlink(mqname);
	psmq->qpub = (mqd_t)-1;
	psmq->qsub = (mqd_t)-1;
	return -1;
}


/* ==========================================================================
    Cleans up whatever has been allocate through the life cycle of 'psmq'.
    Also sends CLOSE command to broker, so it can cleanup and free space for
    another queue.

    Returns 0 on success or -1 on errors

    errno:
            EINVAL      psmq is invalid (null)
            EBADF       psmq has not been initialized
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
	mq_close(psmq->qsub);
	psmq->qpub = (mqd_t) -1;
	psmq->qsub = (mqd_t) -1;
	return 0;
}


/* ==========================================================================
    Subscribes to 'psmq' broker on 'topic'. After this, broker will send
    back ACK message with information whether subscribtion was success or
    not. You can check this by calling psmq_receive() after this function.

    Returns 0 on success or -1 on error

    errno:
            EINVAL      psmq is invalid (null)
            EINVAL      topic is invalid (null)
            EINVAL      topic is empty ("")
            EBADMSG     topic contains only "/" and nothing else
            EBADF       psmq has not been initialized
            ENOBUFS     topic is too long
   ========================================================================== */


int psmq_subscribe
(
	struct psmq  *psmq,  /* psmq object */
	const char   *topic  /* topic to register to */
)
{
	VALID(EINVAL, psmq);
	VALID(EINVAL, topic);
	VALID(EINVAL, topic[0] != '\0');
	VALID(EBADMSG, topic[0] == '/');
	VALID(EBADF, psmq->qsub != (mqd_t)-1);
	VALID(EBADF, psmq->qsub != psmq->qpub);
	VALID(ENOBUFS, strlen(topic) + 1 <= PSMQ_MSG_MAX);

	/* send subscribe request to the server */
	return psmq_publish_msg(psmq, PSMQ_CTRL_CMD_SUBSCRIBE,
			psmq->fd, topic, NULL, 0, 0);
}


/* ==========================================================================
    Unsubscribes from 'topic'. After call to this function, broker will send
    back ACK reply with information whether command was success or not. You
    can check this by calling psmq_receive() after this.

    Returns 0 on success or -1 on error

    errno:
            EINVAL      psmq is invalid (null)
            EINVAL      topic is invalid (null)
            EINVAL      topic is empty ("")
            EBADMSG     topic contains only "/" and nothing else
            EBADF       psmq has not been initialized
            ENOBUFS     topic is too long
   ========================================================================== */


int psmq_unsubscribe
(
	struct psmq  *psmq,  /* psmq object */
	const char   *topic  /* topic to register to */
)
{
	VALID(EINVAL, psmq);
	VALID(EINVAL, topic);
	VALID(EINVAL, topic[0] != '\0');
	VALID(EBADMSG, topic[0] == '/');
	VALID(EBADF, psmq->qsub != (mqd_t)-1);
	VALID(EBADF, psmq->qsub != psmq->qpub);
	VALID(ENOBUFS, strlen(topic) + 1 <= PSMQ_MSG_MAX);

	/* send subscribe request to the server */
	return psmq_publish_msg(psmq, PSMQ_CTRL_CMD_UNSUBSCRIBE,
			psmq->fd, topic, NULL, 0, 0);
}


/* ==========================================================================
    Sends ioctl to alter how broker interfacts with client.

    Returns 0 on success or -1 on error

    errno:
            EINVAL      psmq is invalid (null)
            EINVAL      req is not a valid ioctl request
            EBADF       psmq has not been initialized
   ========================================================================== */


int psmq_ioctl
(
	struct psmq   *psmq,        /* psmq object */
	int            req,         /* ioctl request to make */
	...                         /* variadic data for req */)
{
	int            val_int;     /* ap value treated as integer */
	unsigned short val_ushort;  /* ap value treated as unsigned short */
	va_list        ap;          /* variadic argument */
	char           buf[32];     /* buffer with ioctl data to send to broker */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	VALID(EINVAL, psmq);
	VALID(EBADF, psmq->qsub != (mqd_t)-1);
	VALID(EBADF, psmq->qsub != psmq->qpub);

	va_start(ap, req);
	memset(buf, 0x00, sizeof(buf));
	buf[0] = req;

	switch (req)
	{
	/* ==================================================================
	                       __        __   _                        __
	      ____ ___  ___   / /__ __  / /_ (_)__ _  ___  ___  __ __ / /_
	     / __// -_)/ _ \ / // // / / __// //  ' \/ -_)/ _ \/ // // __/
	    /_/   \__// .__//_/ \_, /  \__//_//_/_/_/\__/ \___/\_,_/ \__/
	             /_/       /___/
	   ================================================================== */

	case PSMQ_IOCTL_REPLY_TIMEOUT:
		/* va_arg cannot accept short as type, so we
		 * need this intermediate step to extract int
		 * first, and then assign it to ushort */
		val_int = va_arg(ap, int);
		VALID(EINVAL, val_int <= USHRT_MAX);

		val_ushort = val_int;
		memcpy(buf + 1, &val_ushort, sizeof(val_ushort));
		return psmq_publish_msg(psmq, PSMQ_CTRL_CMD_IOCTL, psmq->fd, NULL,
				buf, 1 + sizeof(val_ushort), 0);

	default:
		errno = EINVAL;
		return -1;
	}
}


/* ==========================================================================
    Sets time in ms, how long broker will wait for us to take some messages
    from mqueue, in case our mqueue is full.

    Note: if you set this to high value, and you are notoriously slow to get
    messages from the queue, this may severy impact performance of broker,
    as broker is single threaded by design, and will hang when it has to
    wait for client.
   ========================================================================== */


int psmq_ioctl_reply_timeout
(
	struct psmq   *psmq,  /* psmq object */
	unsigned short val    /* timeout value in ms */
)
{
	return psmq_ioctl(psmq, PSMQ_IOCTL_REPLY_TIMEOUT, val);
}
