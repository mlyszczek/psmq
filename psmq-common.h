/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef PSMQ_BROKER_H
#define PSMQ_BROKER_H 1

#ifdef HAVE_CONFIG_H
#   include "psmq-config.h"
#endif

#include <limits.h>
#include <stddef.h>
#include <errno.h>


/* hard limits, these are minimal values that either makes sense or
 * psmq cannot properly work with different values that these or
 * internal types forbids some values to be bigger
 */

#define PSMQ_MAX_CLIENTS_HARD_MAX 999
#define PSMQ_MAX_CLIENTS_HARD_MIN   2
#define PSMQ_PAYLOAD_MAX_HARD_MIN   2
#define PSMQ_TOPIC_MAX_HARD_MIN     2

#if PSMQ_PAYLOAD_MAX < PSMQ_PAYLOAD_MAX_HARD_MIN
    /* couple of control messages will send 2 bytes, for example,
     * open will send ack in byte 1 and file descriptor in byte 2
     */
#   error PSMQ_PAYLOAD_MAX must be at least 2 bytes big
#endif

#if PSMQ_TOPIC_MAX < PSMQ_TOPIC_MAX_HARD_MIN
    /* all topics should start with '/', so it is rather pointless
     * to set this value to 1, you won't create one topic with '/'
     * only, will you?
     */
#   error PSMQ_TOPIC_MAX must be at least 2 bytes big
#endif

#if PSMQ_MAX_CLIENTS > (UCHAR_MAX - 1)
    /* psmq uses unsigned char to hold, and transmit client's file
     * descriptors, so you cannot set max clients to be bigger than
     * what unsigned char can hold. -1 is because UCHAR_MAX is
     * reserved for errors.
     */
#   error PSMQ_MAX_CLIENTS must not be bigger than UCHAR_MAX
#endif

#if PSMQ_MAX_CLIENTS > PSMQ_MAX_CLIENTS_HARD_MAX
    /* some systems might have char size 10bits or bigger, but even
     * them cannot go beyond hard max clients
     */
#   error PSMQ_MAX_CLIENTS must not be bigger than PSMQ_MAX_CLIENTS_HARD_MAX
#endif

#if PSMQ_MAX_CLIENTS < PSMQ_MAX_CLIENTS_HARD_MIN
    /* psmq is a publish subscriber program, so at least one client
     * must publish and one should receive messages, it is really
     * pointless to have only one publisher or one subscriber, thus
     * this error
     */
#   error PSMQ_MAX_CLIENTS must be bigger than PSMQ_MAX_CLIENTS_HARD_MIN
#endif

#define PSMQ_MAX(a, b) ((a) > (b) ? (a) : (b))
#define size_of_member(type, member) sizeof(((type *)0)->member)

#define PSMQ_TOPIC_OPEN        "-o"
#define PSMQ_TOPIC_CLOSE       "-c"
#define PSMQ_TOPIC_SUBSCRIBE   "-s"
#define PSMQ_TOPIC_UNSUBSCRIBE "-u"
#define PSMQ_TOPIC_ENABLE      "-e"
#define PSMQ_TOPIC_DISABLE     "-d"
/* it's 3 because some messages also contains file descriptor in
 * which case, controll topic will be "-X/" after which comes file
 * descriptor
 */
#define PSMQ_CTRL_TOPIC_LEN    3

/* how many space will maximum client take in string, 256 is 3
 * chars, so 3. Also 999 is 3.
 */
#define PSMQ_MAX_CLIENTS_STR_MAX 3

/* this is minimum size of topic to hold essential control messages
 */

#define PSMQ_TOPIC_WITH_FD  (PSMQ_CTRL_TOPIC_LEN + PSMQ_MAX_CLIENTS_STR_MAX)

/* clients publishes to broker messages via this structure, both
 * control frames and ordinary topics are sent with it. Broker
 * creates one queue with sizeof(struct psmq_msg_pub) as msgsize.
 * If you set topic and payload size to some small values, which
 * will result in buffers being to small to carry essential
 * control messages, array in this struct will be enlarged
 * to keep psmq operational. You're welcome.
 */

struct psmq_msg_pub
{
    /* message topic, this should be big enough to hold control topics
     * (2 chars) and file descriptor information in format "/NNN"
     * where NNN is fd, and that is 1 character (for '/') and
     * number of digits calculated from hard limit
     */

    char          topic[PSMQ_MAX(PSMQ_TOPIC_WITH_FD, PSMQ_TOPIC_MAX) + 1];

    /* payload can carry actual payload or topic (for subscribe),
     * so pick whatever is bigger
     */

    unsigned char payload[PSMQ_MAX(PSMQ_PAYLOAD_MAX, PSMQ_TOPIC_MAX + 1)];
    size_t        paylen;
};

/* broker sends messages to clients via this structure. Clients
 * when connecting to broker creates mqueue with sizeof(struct
 * psmq_msg) as msgsize, so it is worth keeping this to minimum,
 * since clients are many, and might send a lot of data between
 * them. There is no real restriction of array sizes, you can set
 * topic size to 1 and payload to 1 too if you wish.
 */

struct psmq_msg
{
    char          topic[PSMQ_TOPIC_MAX + 1];
    unsigned char payload[PSMQ_PAYLOAD_MAX];
    size_t        paylen;
};

#endif /* PSMQ_BROKER_H */
