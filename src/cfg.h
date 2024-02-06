/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef PSMQ_PSMQD_CFG_H
#define PSMQ_PSMQD_CFG_H 1

#include "psmq-common.h"

struct psmqd_cfg
{
#if PSMQ_HAVE_EMBEDLOG
    enum el_level   log_level;
    int             colorful_output;
    const char     *program_log;
#endif
    const char     *broker_name;
    int             broker_maxmsg;
    int             remove_queue;
};

int psmqd_cfg_init(int argc, char *argv[]);
void psmqd_cfg_destroy(void);
void psmqd_cfg_print(void);

#endif /* PSMQ_PSMQD_CFG_H */
