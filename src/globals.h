/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef PSMQ_GLOBALS_H
#define PSMQ_GLOBALS_H 1

#include <embedlog.h>
#include "cfg.h"

extern struct psmqd_cfg   g_psmqd_cfg;
extern struct el_options  g_psmqd_log;
extern int                g_psmqd_shutdown;

#endif /* PSMQ_GLOBALS_H */
