/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include "topic-list.h"

#include <stdlib.h>
#include <embedlog.h>
#include <string.h>
#include <errno.h>

#include "mtest.h"

mt_defs_ext();

/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */



/* ==========================================================================
    Create topic list from given 'topics'
   ========================================================================== */


static struct psmqd_tl *create_list
(
    const char       *topics[]  /* topic to create list with */
)
{
    const char       *topic;    /* current topic */
    struct psmqd_tl  *tl;       /* newly created tl */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    for (tl = NULL, topic = *topics; topic != NULL; topic = *++topics)
    {
        if (psmqd_tl_add(&tl, topic) != 0)
        {
            el_print(ELE, "psmqd_tl_add() failed");
            psmqd_tl_destroy(tl);
            return NULL;
        }
    }

    return tl;
}


/* ==========================================================================
    Checks if 'tl' contains all topics listed in 'topics'. Lists must match
    in 100%, so if tl is "a, b, c" and topics is "a, b" then those lists
    don't match.

    Returns 0 when both lists matches, and -1 otherwise
   ========================================================================== */


static int check_list
(
    struct psmqd_tl  *tl,
    const char       *topics[]
)
{
    const char      **topics_save;
    const char       *topic;
    struct psmqd_tl  *node;
    size_t            tcount;
    size_t            tlcount;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    topics_save = topics;

    /* first calculate number of elements in topics and tl
     */

    for (tcount = 0, topic = *topics; *topics != NULL; ++topics, ++tcount) {}
    for (tlcount = 0, node = tl; node != NULL; node = node->next, ++tlcount) {}

    /* if tcount is different than tlcount, there is no way
     * tl to match topics
     */

    if (tcount != tlcount)
    {
        return -1;
    }

    /* check if all topics exist in the tl list
     */

    topics = topics_save;
    for (topic = *topics; topic != NULL; topic = *++topics)
    {
        for (node = tl; node != NULL; node = node->next)
        {
            if (strcmp(node->topic, topic) == 0)
            {
                /* that topic is in tl list, exit first
                 * loop
                 */

                break;
            }
        }

        if (node == NULL)
        {
            /* we went through whole tl list, and did not
             * stumble upon current topic, so that topic
             * does not exist there. That's right, we
             * rise error!
             */

            return -1;
        }
    }

    /* iterated through all topics and no error so far,
     * that means all topics are in tl list, good, good
     */

    return 0;
}


/* ==========================================================================
                           __               __
                          / /_ ___   _____ / /_ _____
                         / __// _ \ / ___// __// ___/
                        / /_ /  __/(__  )/ /_ (__  )
                        \__/ \___//____/ \__//____/

   ========================================================================== */


static void psmqd_tl_create_new(void)
{
    const char       *topics[] = { "/1", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);
    mt_fok(check_list(tl, topics));
    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_create_multiple(void)
{
    const char       *topics[] = { "/1", "/2", "/3/4/5", "/6/7/8/9", NULL };
    const char       *expected[] = { "/1", "/6/7/8/9", "/3/4/5", "/2", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);
    mt_fok(check_list(tl, expected));
    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_create_and_delete_one_topic(void)
{
    const char       *topics[] = { "/1", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);
    mt_fok(psmqd_tl_delete(&tl, "/1"));
    mt_fail(tl == NULL);
    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_delete_one_first_topic(void)
{
    const char       *topics[] = { "/1", "/2", "/3/4/5", "/6/7/8/9", NULL };
    const char       *expected[] = { "/6/7/8/9", "/3/4/5", "/2", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);
    mt_fok(psmqd_tl_delete(&tl, "/1"));
    mt_fok(check_list(tl, expected));
    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_delete_one_middle_topic(void)
{
    const char       *topics[] = { "/1", "/2", "/3/4/5", "/6/7/8/9", NULL };
    const char       *expected[] = { "/1", "/6/7/8/9", "/2", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);
    mt_fok(psmqd_tl_delete(&tl, "/3/4/5"));
    mt_fok(check_list(tl, expected));
    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_delete_one_last_topic(void)
{
    const char       *topics[] = { "/1", "/2", "/3/4/5", "/6/7/8/9", NULL };
    const char       *expected[] = { "/1", "/6/7/8/9", "/3/4/5", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);
    mt_fok(psmqd_tl_delete(&tl, "/2"));
    mt_fok(check_list(tl, expected));
    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_delete_multi_first_topic(void)
{
    const char       *topics[] = { "/1", "/2", "/3/4/5", "/6/7/8/9", NULL };
    const char       *expected[] = { "/3/4/5", "/2", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);
    mt_fok(psmqd_tl_delete(&tl, "/1"));
    mt_fok(psmqd_tl_delete(&tl, "/6/7/8/9"));
    mt_fok(check_list(tl, expected));
    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_delete_multi_middle_topic(void)
{
    const char       *topics[] = { "/1", "/2", "/3/4/5", "/6/7/8/9", NULL };
    const char       *expected[] = { "/1", "/2", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);
    mt_fok(psmqd_tl_delete(&tl, "/3/4/5"));
    mt_fok(psmqd_tl_delete(&tl, "/6/7/8/9"));
    mt_fok(check_list(tl, expected));
    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_delete_multi_last_topic(void)
{
    const char       *topics[] = { "/1", "/2", "/3/4/5", "/6/7/8/9", NULL };
    const char       *expected[] = { "/1", "/6/7/8/9", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);
    mt_fok(psmqd_tl_delete(&tl, "/2"));
    mt_fok(psmqd_tl_delete(&tl, "/3/4/5"));
    mt_fok(check_list(tl, expected));
    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_delete_multi_mixed_topic(void)
{
    const char       *topics[] = { "/1", "/2", "/3", "/4", "/5", "/6",
                                   "/7", "/8", "/9", "/10", "/11", NULL };
    const char       *expected[] = { "/9", "/7", "/5", "/4", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);
    mt_fok(psmqd_tl_delete(&tl, "/2"));
    mt_fok(psmqd_tl_delete(&tl, "/1"));
    mt_fok(psmqd_tl_delete(&tl, "/10"));
    mt_fok(psmqd_tl_delete(&tl, "/8"));
    mt_fok(psmqd_tl_delete(&tl, "/11"));
    mt_fok(psmqd_tl_delete(&tl, "/3"));
    mt_fok(psmqd_tl_delete(&tl, "/6"));
    mt_fok(check_list(tl, expected));
    mt_fok(psmqd_tl_delete(&tl, "/9"));
    mt_fok(psmqd_tl_delete(&tl, "/7"));
    mt_fok(psmqd_tl_delete(&tl, "/5"));
    mt_fok(psmqd_tl_delete(&tl, "/4"));
    mt_fail(tl == NULL);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_destroy_multi_topic(void)
{
    const char       *topics[] = { "/1", "/2", "/3", "/4", "/5", "/6",
                                   "/7", "/8", "/9", "/10", "/11", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);

    /* if destroy fails to free all memory, it will be seen in
     * valgrind, not here!
     */

    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_delete_nonexisting_topic(void)
{
    const char       *topics[] = { "/1", "/2", "/3", "/4", "/5", "/6",
                                   "/7", "/8", "/9", "/10", "/11", NULL };
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    mt_assert((tl = create_list(topics)) != NULL);
    mt_ferr(psmqd_tl_delete(&tl, "/doesnt/exit"), ENOENT);

    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_add_null_topic(void)
{
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    tl = NULL;
    mt_ferr(psmqd_tl_add(&tl, NULL), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_delete_null_topic(void)
{
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    tl = NULL;
    psmqd_tl_add(&tl, "/a");
    mt_ferr(psmqd_tl_delete(&tl, NULL), EINVAL);
    psmqd_tl_destroy(tl);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_delete_null_list(void)
{
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    tl = NULL;
    mt_ferr(psmqd_tl_delete(&tl, "/a"), ENOENT);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_delete_null_null_list(void)
{
    mt_ferr(psmqd_tl_delete(NULL, "/a"), EINVAL);
}


/* ==========================================================================
   ========================================================================== */


static void psmqd_tl_destroy_null_list(void)
{
    struct psmqd_tl  *tl;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    tl = NULL;
    mt_ferr(psmqd_tl_destroy(tl), EINVAL);
}


/* ==========================================================================
             __               __
            / /_ ___   _____ / /_   ____ _ _____ ____   __  __ ____
           / __// _ \ / ___// __/  / __ `// ___// __ \ / / / // __ \
          / /_ /  __/(__  )/ /_   / /_/ // /   / /_/ // /_/ // /_/ /
          \__/ \___//____/ \__/   \__, //_/    \____/ \__,_// .___/
                                 /____/                    /_/
   ========================================================================== */


void psmqd_tl_test_group(void)
{
    mt_run(psmqd_tl_create_new);
    mt_run(psmqd_tl_create_multiple);
    mt_run(psmqd_tl_create_and_delete_one_topic);
    mt_run(psmqd_tl_delete_one_first_topic);
    mt_run(psmqd_tl_delete_one_middle_topic);
    mt_run(psmqd_tl_delete_one_last_topic);
    mt_run(psmqd_tl_delete_multi_first_topic);
    mt_run(psmqd_tl_delete_multi_middle_topic);
    mt_run(psmqd_tl_delete_multi_last_topic);
    mt_run(psmqd_tl_delete_multi_mixed_topic);
    mt_run(psmqd_tl_delete_nonexisting_topic);
    mt_run(psmqd_tl_destroy_multi_topic);
    mt_run(psmqd_tl_add_null_topic);
    mt_run(psmqd_tl_delete_null_topic);
    mt_run(psmqd_tl_delete_null_list);
    mt_run(psmqd_tl_delete_null_null_list);
    mt_run(psmqd_tl_destroy_null_list);
}
