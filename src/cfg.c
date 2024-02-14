/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         ------------------------------------------------------------
        / module parses options passed via argc and argv and creates \
        \ config object based on these values                        /
         ------------------------------------------------------------
         \     /\  ___  /\
          \   // \/   \/ \\
             ((    O O    ))
              \\ /     \ //
               \/  | |  \/
                |  | |  |
                |  | |  |
                |   o   |
                | |   | |
                |m|   |m|
   ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#ifdef HAVE_CONFIG_H
#   include "psmq-config.h"
#endif

#include <embedlog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cfg.h"
#include "globals.h"
#include "psmq-common.h"
#include "psmq.h"


/* ==========================================================================
          __             __                     __   _
     ____/ /___   _____ / /____ _ _____ ____ _ / /_ (_)____   ____   _____
    / __  // _ \ / ___// // __ `// ___// __ `// __// // __ \ / __ \ / ___/
   / /_/ //  __// /__ / // /_/ // /   / /_/ // /_ / // /_/ // / / /(__  )
   \__,_/ \___/ \___//_/ \__,_//_/    \__,_/ \__//_/ \____//_/ /_//____/

   ========================================================================== */


#define EL_OPTIONS_OBJECT &g_psmqd_log


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


static int cfg_parse_args
(
	int    argc,   /* number of arguments in argv */
	char  *argv[]  /* argument list */
)
{
	/* macros to parse arguments in switch(opt) block */


	/* check if optarg is between MINV and MAXV values and if so,
	 * store converted optarg in to config.OPTNAME field. If error
	 * occurs, force function to return with -1 error */
#   define PARSE_INT(OPTNAME, MINV, MAXV) \
	{ \
		long   val;     /* value converted from OPTARG */ \
		char  *endptr;  /* pointer for errors fron strtol */ \
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/ \
		\
		val = strtol(optarg, &endptr, 10); \
		if (*endptr != '\0') \
		{ \
			/* error occured */ \
			fprintf(stderr, "wrong value '%s' for option '%s\n", \
					optarg, #OPTNAME); \
			return -1; \
		} \
		\
		if (val < MINV || MAXV < val) \
		{ \
			/* number is outside of defined domain */ \
			fprintf(stderr, "value for '%s' should be between %d and %d\n", \
					#OPTNAME, MINV, MAXV); \
			return -1; \
		} \
		\
		g_psmqd_cfg.OPTNAME = val; \
	}

	int  arg;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	optind = 1;
	while ((arg = getopt(argc, argv, ":vhl:dcp:m:b:r")) != -1)
	{
		switch (arg)
		{
		case 'c': g_psmqd_cfg.colorful_output = 1; break;
		case 'l': PARSE_INT(log_level, 0, 7); break;
		case 'p': g_psmqd_cfg.program_log = optarg; break;

		case 'm': PARSE_INT(broker_maxmsg, 0, INT_MAX); break;
		case 'b': g_psmqd_cfg.broker_name = optarg; break;
		case 'r': g_psmqd_cfg.remove_queue = 1; break;

		case 'h':
			printf(
					"psmqd - broker for publish subscribe over mqueue\n"
					"\n"
					"Usage: %s [-h | -v | options]\n"
					"\n", argv[0]);
			printf(
					"options:\n"
					"\t-h           print this help and exit\n"
					"\t-v           print version and exit\n"
					"\t-c           enable nice colors for logs\n"
					"\t-l<level>    logging level 0-7\n"
					"\t-p<path>     where logs will be stored (stdout if not specified)\n");
			printf(
					"\t-b<name>     name for broker control queue\n"
					"\t-r           if set, control queue will be removed before starting\n"
					"\t-m<maxmsg>   max messages on broker control queue\n"
					"\n");
			printf(
					"logging levels:\n"
					"\t0         fatal errors, application cannot continue\n"
					"\t1         major failure, needs immediate attention\n"
					"\t2         critical errors\n"
					"\t3         error but recoverable\n"
					"\t4         warnings\n"
					"\t5         normal message, but of high importance\n"
					"\t6         info log, doesn't print that much (default)\n"
					"\t7         debug, not needed in production\n"
					"\n");
			return -2;

		case 'v':
			printf("psmqd "PACKAGE_VERSION"\n"
					"by Michał Łyszczek <michal.lyszczek@bofc.pl>\n");

			return -3;

		case ':':
			fprintf(stderr, "option -%c requires an argument\n", optopt);
			return -4;

		case '?':
			fprintf(stdout, "unknown option -%c\n", optopt);
			return -5;

		default:
			fprintf(stderr, "unexpected return from getopt '0x%02x'\n", arg);
			return -6;
	}
}

	return 0;

#   undef PARSE_INT
}


/* ==========================================================================
                       __     __ _          ____
        ____   __  __ / /_   / /(_)_____   / __/__  __ ____   _____ _____
       / __ \ / / / // __ \ / // // ___/  / /_ / / / // __ \ / ___// ___/
      / /_/ // /_/ // /_/ // // // /__   / __// /_/ // / / // /__ (__  )
     / .___/ \__,_//_.___//_//_/ \___/  /_/   \__,_//_/ /_/ \___//____/
    /_/
   ========================================================================== */


int psmqd_cfg_init
(
	int    argc,   /* number of arguments in argv */
	char  *argv[]  /* argument list */
)
{
	/* disable error printing from getopt library */
#if PSMQ_NO_OPTERR == 0
	opterr = 0;
#endif

	/* set g_psmqd_cfg object to well-known default state */
	memset(&g_psmqd_cfg, 0x00, sizeof(g_psmqd_cfg));
	g_psmqd_cfg.log_level = EL_INFO;
	g_psmqd_cfg.broker_maxmsg = 10;
	g_psmqd_cfg.broker_name = "/psmqd";

	/* parse options from command line argument
	 * overwritting default ones */
	return cfg_parse_args(argc, argv);
}


/* ==========================================================================
    Prints configuration to default logging facility
   ========================================================================== */


void psmqd_cfg_print(void)
{
	/* macro for easy field printing */

#define CONFIG_PRINT(field, type) \
	el_oprint(OELN, "%s%s "type, #field, padder + strlen(#field), \
			g_psmqd_cfg.field)

#define CONFIG_PRINT_VAR(var, type) \
		el_oprint(OELN, "%s%s "type, #var, padder + strlen(#var), var)

	char padder[] = "............................:";
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


	el_oprint(OELN, PACKAGE_STRING);
	el_oprint(OELN, "psmqd configuration");
	CONFIG_PRINT(log_level, "%d");
	CONFIG_PRINT(colorful_output, "%d");
	if (g_psmqd_cfg.program_log)
		CONFIG_PRINT(program_log, "%s");
	else
		CONFIG_PRINT(program_log, "(stderr)");
	CONFIG_PRINT(broker_name, "%s");
	CONFIG_PRINT(broker_maxmsg, "%d");
	CONFIG_PRINT(remove_queue, "%d");
	CONFIG_PRINT_VAR(PSMQ_MSG_MAX, "%u");
	CONFIG_PRINT_VAR(sizeof(struct psmq_msg), "%zu");

#undef CONFIG_PRINT
}
