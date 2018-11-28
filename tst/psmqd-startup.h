/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef PSMQD_TEST_STARTUP
#define PSMQD_TEST_STARTUP

#include "psmq-common.h"
#include <pthread.h>

#define QNAME_LEN size_of_member(struct psmq_msg_pub, payload)


extern pthread_t    gt_psmqd_t;
extern char         gt_broker_name[QNAME_LEN];
extern struct psmq  gt_pub_psmq;
extern struct psmq  gt_sub_psmq;
extern char         gt_pub_name[QNAME_LEN];
extern char         gt_sub_name[QNAME_LEN];


void psmqt_gen_random_string(char *s, size_t l);
char * psmqt_gen_queue_name(char *s, size_t l);
void psmqt_gen_unique_queue_name_array(void *array, size_t alen, size_t qlan);
int psmqt_msg_receiver(char *topic, void *payload, size_t paylen,
    unsigned int prio, void *arg);
void psmqt_prepare_test(void);
void psmqt_prepare_test_with_clients(void);
void psmqt_cleanup_test(void);
void psmqt_cleanup_test_with_clients(void);

#endif /* PSMQD_TEST_STARTUP */
