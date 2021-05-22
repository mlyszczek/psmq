/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef PSMQ_BROKER_H
#define PSMQ_BROKER_H 1

#ifdef HAVE_CONFIG_H
#   include "psmq-config.h"
#endif

#include <limits.h>
#include <stddef.h>
#include <sys/time.h>

/* hard limits, these are minimal values that either makes sense or
 * psmq cannot properly work with different values that these or
 * internal types forbids some values to be bigger */

#if PSMQ_MAX_CLIENTS > (UCHAR_MAX - 1)
	/* psmq uses unsigned char to hold, and transmit client's file
	 * descriptors, so you cannot set max clients to be bigger than
	 * what unsigned char can hold. -1 is because UCHAR_MAX is
	 * reserved for errors. */
#   error PSMQ_MAX_CLIENTS must not be bigger than (UCHAR_MAX - 1)
#endif

#define PSMQ_MAX_CLIENTS_HARD_MIN 2
#if PSMQ_MAX_CLIENTS < PSMQ_MAX_CLIENTS_HARD_MIN
	/* psmq is a publish subscriber program, so at least one client
	 * must publish and one should receive messages, it is really
	 * pointless to have only one publisher or one subscriber, thus
	 * this error */
#   error PSMQ_MAX_CLIENTS must be bigger than 1
#endif

#define size_of_member(type, member) sizeof(((type *)0)->member)
/* calculates real size of msg to send over, real that is, if
 * data[PSMQ_MSG_MAX] is 50, topic is 10 bytes long and payload is 4
 * bytes long, there is no need to send whole struct with 50 bytes,
 * when more than half of it are going to be useless bytes. This macro
 * will calculate number of bytes that are actual usefull and that
 * should be transfered over. Note: m argument, must be validated,
 * that is, data[] must have at least on null termination (topic)
 * or paylen must be 0 and data[0] = '\0'.
 *
 * only exception from the rule is ioctl control message, since
 * it does not transport topic but only custom binary data, we
 * cannot run strlen() on this and simply use paylen as length
 * of payload */
#define psmq_real_msg_size(m) (sizeof((m).paylen) + sizeof((m).ctrl) + \
		((m).ctrl.cmd == PSMQ_CTRL_CMD_IOCTL ? 0 : (strlen((m).data) + 1)) + \
		(m).paylen)


void psmq_ms_to_tp(size_t ms, struct timespec *tp);

#endif /* PSMQ_BROKER_H */
