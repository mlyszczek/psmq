/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <string.h>

int psmq_sub_main(int argc, char *argv[]);
int psmq_pub_main(int argc, char *argv[]);

static int start_psmq_pub(const struct shell *shell, int argc, char *argv[])
{
	return psmq_pub_main(argc, argv);
}

void psmq_sub_main_thread(void *arg1, void *arg2, void *arg3)
{
	(void)arg3;

	int *argc;
	char **argv;

	argc = arg1;
	argv = arg2;

	psmq_sub_main(*argc, argv);
	for (int i = 0; i != *argc; i++)
		free(argv[i]);
	free(argv);
	free(argc);
}

K_THREAD_STACK_DEFINE(g_psmq_sub_stack, CONFIG_PSMQ_SUB_STACKSIZE);
struct k_thread thread_data;
static int start_psmq_sub(const struct shell *shell, int argc, char *argv[])
{
	char **argv_copy;
	int *argc_copy;


	argc_copy = malloc(sizeof(*argc_copy));
	*argc_copy = argc;

	argv_copy = malloc(argc * sizeof(*argv_copy));
	for (int i = 0; i != argc; i++)
		argv_copy[i] = strdup(argv[i]);

	k_thread_create(&thread_data, g_psmq_sub_stack,
			K_THREAD_STACK_SIZEOF(g_psmq_sub_stack),
			psmq_sub_main_thread,
			argc_copy, argv_copy, NULL,
			CONFIG_PSMQ_SUB_PRIORITY, 0, K_NO_WAIT);
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	g_psmq_cmds,
#if CONFIG_PSMQ_TOOLS_SUB
	SHELL_CMD_ARG(sub, NULL,
"listen to subscribed messages over psmq\n"
"\n"
"usage: \n"
"\tpsmq sub [-h | -v]\n"
"\tpsmq sub <-t topic> <[-t topic]>\n"
"\n"
"\t-h                   shows help and exit\n"
"\t-v                   shows version and exit\n"
"\t-t <topic>           topic to subscribe to, can be used multiple times\n"
, start_psmq_sub, 1, 10),
#endif
#if CONFIG_PSMQ_TOOLS_PUB
	SHELL_CMD_ARG(pub, NULL,
"publish message over psmq\n"
"\n"
"usage: \n"
"\tpsmq pub [-h | -v]\n"
"\tpsmq pub -t <topic> <-m <message> | -e> \n"
"\n"
"\n"
"\t-h               print this help and exit\n"
"\t-v               print version and exit\n"
"\t-t <topic>       topic on which message should be published\n"
"\t-m <message>     message to publish\n"
"\t-e               publish message without payload on topic\n"
"\n"
"When message is read from stdin, program will send each line as separate\n"
"message on passed topic until EOF is encoutered\n"
		, start_psmq_pub, 1, 10),
#endif
	SHELL_SUBCMD_SET_END
);


SHELL_CMD_REGISTER(psmq, &g_psmq_cmds, "psmq", NULL);
