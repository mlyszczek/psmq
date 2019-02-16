/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         -------------------------------------------------------------
        / main entry point for psmq broker, it initializes and starts \
        \ broker so it can process requests from clients.             /
         -------------------------------------------------------------
                             \
                              \
                  oO)-.        \              .-(Oo
                 /__  _\                     /_  __\
                 \  \(  |     ()~()         |  )/  /
                  \__|\ |    (-___-)        | /|__/
                  '  '--'    ==`-'==        '--'  '
   ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include <embedlog.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#if PSMQ_NO_SIGNALS == 0
#   include <signal.h>
#endif

#include "globals.h"
#include "broker.h"


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    Handler when SIGINT or SIGTERM are received by the program
   ========================================================================== */

#if PSMQ_NO_SIGNALS == 0

static void sigint_handler
(
    int signo   /* signal that triggered this handler */
)
{
    (void)signo;

    g_psmqd_shutdown = 1;
}

#endif


/* ==========================================================================
                                              _
                           ____ ___   ____ _ (_)____
                          / __ `__ \ / __ `// // __ \
                         / / / / / // /_/ // // / / /
                        /_/ /_/ /_/ \__,_//_//_/ /_/

   ========================================================================== */


#if PSMQ_STANDALONE
int main
#else
int psmqd_main
#endif
(
    int               argc,    /* number of arguments in argv */
    char             *argv[]   /* arguments from command line */
)
{
#if PSMQ_NO_SIGNALS == 0
    {
        struct sigaction  sa;      /* signal action instructions */
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


        /* install signal handler to nicely exit program
         */

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sigint_handler;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
    }
#endif

    g_psmqd_shutdown = 0;

    switch (psmqd_cfg_init(argc, argv))
    {
    case 0:
        /* no errors in parsing arguments, continue program execution
         */

        break;

    case -2:
    case -3:
        /* help or version was printed, exit program without error
         */

        return 0;

    default:
        /* error occured when parsing arguments, die
         */

        return 1;
    }

#if PSMQ_ENABLE_DAEMON
    if (g_psmqd_cfg.daemonize)
    {
        daemonize(g_psmqd_cfg.pid_file, g_psmqd_cfg.user, g_psmqd_cfg.group);
    }
#endif

    /* configure logger for diagnostic logs
     */

    el_oinit(&g_psmqd_log);
    el_ooption(&g_psmqd_log, EL_LEVEL, g_psmqd_cfg.log_level);
    el_ooption(&g_psmqd_log, EL_TS, EL_TS_LONG);
    el_ooption(&g_psmqd_log, EL_TS_TM, EL_TS_TM_REALTIME);
    el_ooption(&g_psmqd_log, EL_FINFO, 1);
    el_ooption(&g_psmqd_log, EL_COLORS, g_psmqd_cfg.colorful_output);
    el_ooption(&g_psmqd_log, EL_FILE_SYNC_EVERY, 0);
    el_ooption(&g_psmqd_log, EL_OUT, EL_OUT_STDERR);

    if (g_psmqd_cfg.program_log)
    {
        /* save logs to file if that file is specified
         */

        el_ooption(&g_psmqd_log, EL_OUT, EL_OUT_FILE);

        if (el_ooption(&g_psmqd_log, EL_FPATH, g_psmqd_cfg.program_log) != 0)
        {
            fprintf(stderr, "w/couldn't open program log file %s: %s "
                "logs will be printed to stderr\n",
                g_psmqd_cfg.program_log,  strerror(errno));
            el_ooption(&g_psmqd_log, EL_OUT, EL_OUT_STDERR);
        }
    }

    psmqd_cfg_print();

    if (psmqd_broker_init() != 0)
    {
        el_oprint(ELF, &g_psmqd_log, "failed to initialize broker");
        goto broker_init_error;
    }

    psmqd_broker_start();
    psmqd_broker_cleanup();

    el_oprint(ELN, &g_psmqd_log, "exiting psmqd");

broker_init_error:
    el_ocleanup(&g_psmqd_log);
    return 0;
}
