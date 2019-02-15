/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         ------------------------------------------------------------
        / helper program that allows publishing message to specified \
        \ broker from command line or script                         /
         ------------------------------------------------------------
             \
              \
                  oO)-.                       .-(Oo
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


#ifdef HAVE_CONFIG_H
#   include "psmq-config.h"
#endif

#include "psmq.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "psmq-common.h"


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    Publishes single string 'messages' on 'topic' to 'psmq'
   ========================================================================== */


static void send_message
(
    struct psmq  *psmq,     /* psmq object */
    const char   *topic,    /* topic of the message */
    const char   *message,  /* payload of the message */
    unsigned int  prio      /* message priority */
)
{
    /* message set in program arguments, simply send message and
     * exit
     */

    if (psmq_publish(psmq, topic, message, strlen(message) + 1, prio) != 0)
    {
        switch (errno)
        {
        case EINVAL:
            fprintf(stderr, "f/failed to publish, invalid prio %u\n", prio);
            break;

        case EBADMSG:
            fprintf(stderr, "f/failed to publish to %s invalid topic\n",
                topic);
            break;

        case ENOBUFS:
            fprintf(stderr, "f/topic or message is too long\n");
            break;
        }

        return;
    }
}


/* ==========================================================================
    Sends whatever shows on the stdin on 'topic' to 'psmq' broker. Invokes
    one psmq_publish() per read line.
   ========================================================================== */


static void send_stdin
(
    struct psmq  *psmq,   /* psmq object */
    const char   *topic,  /* topic of the message */
    unsigned int  prio    /* message priority */
)
{
    char          line[PSMQ_PAYLOAD_MAX];  /* single line read from stdin */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* message was not set, so we take a little longer path, read
     * stdin until EOF is encoutered and send each line in separate
     * line
     */

    for (;;)
    {
        /* set last byte of line buffer to something other than
         * '\0' to know whether fgets overwrite it or not
         */

        line[sizeof(line) - 1] = 0x55;
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            if (feof(stdin))
            {
                /* end of file reached and no data has been read,
                 * we're done
                 */

                return;
            }

            perror("f/error reading stdin");
            return;
        }

        if (line[sizeof(line) - 1] == '\0' && line[sizeof(line) - 2] != '\n')
        {
            /* fgets did overwrite last byte with '\0' which means
             * it filled whole line buffer
             *
             * and
             *
             * last character in string is not new line
             *
             * which means, user provided line that is too long to
             * fit into one message, abort with error
             */

            fprintf(stderr, "f/line is too long, max line is %lu\n",
                (long unsigned)sizeof(line) - 2);
            return;
        }

        /* now we have full line in buffer, ship it. -1 is just
         * so publish don't send newline character
         */

        if (psmq_publish(psmq, topic, line, strlen(line) + 1, prio) != 0)
        {
            switch (errno)
            {
            case EINVAL:
                fprintf(stderr, "f/failed to publish, invalid prio %u\n", prio);
                break;

            case EBADMSG:
                fprintf(stderr, "f/failed to publish to %s invalid topic\n",
                        topic);
                break;

            case ENOBUFS:
                fprintf(stderr, "f/topic is too long\n");
                break;
            }

            return;
        }
    }
}


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
int psmq_pub_main
#endif
(
    int          argc,         /* number of arguments in argv */
    char        *argv[]        /* arguments from command line */
)
{
    int          arg;          /* arg for getopt() */
    unsigned int prio;         /* message priority */
    const char  *broker_name;  /* queue name of the broker */
    const char  *qname;        /* name of the client queue */
    const char  *message;      /* single message to send (-m parameter) */
    const char  *topic;        /* topic to send message to (-t parameter) */
    struct psmq  psmq;         /* psmq object */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    broker_name = NULL;
    message = NULL;
    topic = NULL;
    qname = NULL;
    prio = 0;

    /* read input arguments
     */

    optind = 1;
    while ((arg = getopt(argc, argv, ":hvt:b:m:n:p:")) != -1)
    {
        switch (arg)
        {
        case 'b': broker_name = optarg; break;
        case 'm': message = optarg; break;
        case 't': topic = optarg; break;
        case 'n': qname = optarg; break;
        case 'p': prio = atoi(optarg); break;
        case 'v':
            printf("%s v"PACKAGE_VERSION"\n"
                "by Michał Łyszczek <michal.lyszczek@bofc.pl>\n", argv[0]);

            return 1;

        case 'h':
            printf(
"%s - publish message over psmq\n"
"\n"
"usage: \n"
"\t%s [-h | -v]\n"
"\t%s -b <name> -t <topic> [-m <message>] [-n <mqueue-name>] [-p <prio>]"
"\n", argv[0], argv[0], argv[0]);
            printf("\n"
"\t-h               print this help and exit\n"
"\t-v               print version and exit\n"
"\t-b <name>        name of the broker (with leading '/' - like '/qname')\n"
"\t-t <topic>       topic on which message should be published\n"
"\t-m <message>     message to publish, if not set read from stdin\n"
"\t-n <mqueue-name> mqueue name to use by pub to receive data from broker\n"
"\t                 if not set, default /psmq_pub will be used\n"
"\t-t <prio>        message priority, must be int, default: 0\n");
            printf("\n"
"When message is read from stdin, program will send each line as separate\n"
"message on passed topic until EOF is encoutered\n");

            return 1;

        case ':':
            fprintf(stderr, "f/option -%c requires an argument\n", optopt);
            return 1;

        case '?':
            fprintf(stderr, "f/unknown option -%c\n", optopt);
            return 1;
        }
    }

    /* validate arguments
     */

    if (broker_name == NULL)
    {
        fprintf(stderr, "f/missing broker name (-b) option\n");
        return 1;
    }

    if (topic == NULL)
    {
        fprintf(stderr, "f/missing topic (-t) option\n");
        return 1;
    }

    if (qname == NULL)
    {
        /* queue name not set, so use default
         */

        qname = "/psmq_pub";
    }

    /* now the action can start
     */

    if (psmq_init(&psmq, broker_name, qname, 2) != 0)
    {
        switch(errno)
        {
        case ENOENT:
            fprintf(stderr, "f/broker %s doesn't exist\n", broker_name);
            break;

        case EINVAL:
            fprintf(stderr, "f/broker or queue name is invalid\n");
            break;

        case ENAMETOOLONG:
            fprintf(stderr, "f/queue name is too long (%lu), max is %lu\n",
                (unsigned long)strlen(qname),
                (unsigned long)size_of_member(struct psmq_msg_pub, payload) - 1);
            break;
        }

        return 1;
    }

    if (message)
    {
        send_message(&psmq, topic, message, prio);
    }
    else
    {
        send_stdin(&psmq, topic, prio);
    }

    psmq_cleanup(&psmq);
    mq_unlink(qname);
    return 0;
}
