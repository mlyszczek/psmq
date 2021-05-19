/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */


/* ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include "cfg.h"
#include "globals.h"
#include "mtest.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>


mt_defs_ext();


/* ==========================================================================
                           __               __
                          / /_ ___   _____ / /_ _____
                         / __// _ \ / ___// __// ___/
                        / /_ /  __/(__  )/ /_ (__  )
                        \__/ \___//____/ \__//____/

   ========================================================================== */


/* ==========================================================================
   ========================================================================== */


static void cfg_all_default(void)
{
	char  *argv[] = { "psmqd" };
	psmqd_cfg_init(1, argv);

	mt_fail(g_psmqd_cfg.log_level == EL_INFO);
	mt_fail(g_psmqd_cfg.colorful_output == 0);
	mt_fail(g_psmqd_cfg.remove_queue == 0);
	mt_fail(g_psmqd_cfg.broker_maxmsg == 10);
	mt_fail(strcmp(g_psmqd_cfg.broker_name, "/psmqd") == 0);
	mt_fail(g_psmqd_cfg.program_log == NULL);
}


/* ==========================================================================
   ========================================================================== */


static void cfg_short_opts(void)
{
	char *argv[] =
	{
		"kurload",
		"-l4",
		"-c",
		"-p", "/var/log/psmqd",
		"-b/brokeros",
		"-m1337",
		"-r"
	};
	int argc = sizeof(argv) / sizeof(const char *);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	psmqd_cfg_init(argc, argv);

	mt_fail(g_psmqd_cfg.log_level == 4);
	mt_fail(g_psmqd_cfg.colorful_output == 1);
	mt_fail(g_psmqd_cfg.remove_queue == 1);
	mt_fail(g_psmqd_cfg.broker_maxmsg == 1337);
	mt_fail(strcmp(g_psmqd_cfg.broker_name, "/brokeros") == 0);
	mt_fail(strcmp(g_psmqd_cfg.program_log, "/var/log/psmqd") == 0);
}


/* ==========================================================================
   ========================================================================== */


static void cfg_mixed_opts(void)
{
	char *argv[] =
	{
		"kurload",
		"-l4",
		"-p", "/var/log/psmqd",
		"-m1337"
	};
	int argc = sizeof(argv) / sizeof(const char *);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	psmqd_cfg_init(argc, argv);

	mt_fail(g_psmqd_cfg.log_level == 4);
	mt_fail(g_psmqd_cfg.colorful_output == 0);
	mt_fail(g_psmqd_cfg.remove_queue == 0);
	mt_fail(g_psmqd_cfg.broker_maxmsg == 1337);
	mt_fail(strcmp(g_psmqd_cfg.broker_name, "/psmqd") == 0);
	mt_fail(strcmp(g_psmqd_cfg.program_log, "/var/log/psmqd") == 0);
}


/* ==========================================================================
   ========================================================================== */


static void cfg_print_help(void)
{
	int    argc = 2;
	char  *argv[] = { "psmqd", "-h", NULL };
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	mt_fail(psmqd_cfg_init(argc, argv) == -2);
}


/* ==========================================================================
   ========================================================================== */


static void cfg_print_version(void)
{
	int    argc = 2;
	char  *argv[] = { "psmqd", "-v", NULL };
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	mt_fail(psmqd_cfg_init(argc, argv) == -3);
}


/* ==========================================================================
   ========================================================================== */


static void cfg_missing_argument(void)
{
	int    argc = 2;
	char  *argv[] = { "psmqd", "-b", NULL };
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	mt_fail(psmqd_cfg_init(argc, argv) == -4);
}


/* ==========================================================================
   ========================================================================== */


static void cfg_unknown_option(void)
{
	int    argc = 2;
	char  *argv[] = { "psmqd", "-X", NULL };
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	mt_fail(psmqd_cfg_init(argc, argv) == -5);
}


/* ==========================================================================
             __               __
            / /_ ___   _____ / /_   ____ _ _____ ____   __  __ ____
           / __// _ \ / ___// __/  / __ `// ___// __ \ / / / // __ \
          / /_ /  __/(__  )/ /_   / /_/ // /   / /_/ // /_/ // /_/ /
          \__/ \___//____/ \__/   \__, //_/    \____/ \__,_// .___/
                                 /____/                    /_/
   ========================================================================== */


void psmqd_cfg_test_group()
{
	mt_run(cfg_all_default);
	mt_run(cfg_short_opts);
	mt_run(cfg_mixed_opts);
	mt_run(cfg_print_help);
	mt_run(cfg_print_version);
	mt_run(cfg_missing_argument);
	mt_run(cfg_unknown_option);
}
