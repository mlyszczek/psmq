/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef PSMQ_H
#define PSMQ_H 1

#include <mqueue.h>

typedef int (*psmq_sub_clbk)(char *topic, void *payload, size_t paylen,
		unsigned int prio, void *userdata);

struct psmq
{
	mqd_t          qsub;
	mqd_t          qpub;
	unsigned char  fd;
	unsigned char  enabled;
};

int psmq_init(struct psmq *psmq, const char *brokername, const char *mqname,
		int maxmsg);
int psmq_cleanup(struct psmq *psmq);
int psmq_publish(struct psmq *psmq, const char *topic, const void *payload,
		size_t paylen, unsigned int prio);
int psmq_enable(struct psmq *psmq, int enable);
int psmq_disable_threaded(struct psmq *psmq);
int psmq_receive(struct psmq *psmq, psmq_sub_clbk clbk, void *userdata);
int psmq_timedreceive(struct psmq *psmq, psmq_sub_clbk clbk, void *userdata,
		struct timespec *tp);
int psmq_timedreceive_ms(struct psmq *psmq, psmq_sub_clbk clbk, void *userdata,
		size_t ms);
int psmq_subscribe(struct psmq *psmq, const char *topic);
int psmq_unsubscribe(struct psmq *psmq, const char *topic);

#endif /* PSMQ_H */
