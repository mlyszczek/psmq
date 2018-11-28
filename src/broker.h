/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef PSMQ_BROKER_H
#define PSMQ_BROKER_H 1

int psmqd_broker_init(void);
int psmqd_broker_start(void);
int psmqd_broker_cleanup(void);

#endif /* PSMQ_BROKER_H */
