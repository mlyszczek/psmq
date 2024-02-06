/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef PSMQ_GLOBALS_H
#define PSMQ_GLOBALS_H 1

#if PSMQ_HAVE_EMBEDLOG
#  include <embedlog.h>
#endif
#include "cfg.h"

#if PSMQ_HAVE_EMBEDLOG
extern struct el          g_psmqd_log;
#endif
extern struct psmqd_cfg   g_psmqd_cfg;
extern int                g_psmqd_shutdown;

#endif /* PSMQ_GLOBALS_H */
