/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ==========================================================================
         ------------------------------------------------------------
        / tl - topic list, a very minimal linked list implementation \
        \ that holds subscribed topics                               /
         ------------------------------------------------------------
          \
           \ ,   _ ___.--'''`--''//-,-_--_.
              \`"' ` || \\ \ \\/ / // / ,-\\`,_
             /'`  \ \ || Y  | \|/ / // / - |__ `-,
            /@"\  ` \ `\ |  | ||/ // | \/  \  `-._`-,_.,
           /  _.-. `.-\,___/\ _/|_/_\_\/|_/ |     `-._._)
           `-'``/  /  |  // \__/\__  /  \__/ \
                `-'  /-\/  | -|   \__ \   |-' |
                  __/\ / _/ \/ __,-'   ) ,' _|'
                 (((__/(((_.' ((___..-'((__,'
   ==========================================================================
          _               __            __         ____ _  __
         (_)____   _____ / /__  __ ____/ /___     / __/(_)/ /___   _____
        / // __ \ / ___// // / / // __  // _ \   / /_ / // // _ \ / ___/
       / // / / // /__ / // /_/ // /_/ //  __/  / __// // //  __/(__  )
      /_//_/ /_/ \___//_/ \__,_/ \__,_/ \___/  /_/  /_//_/ \___//____/

   ========================================================================== */


#include "topic-list.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "valid.h"


/* ==========================================================================
                  _                __           ____
    ____   _____ (_)_   __ ____ _ / /_ ___     / __/__  __ ____   _____ _____
   / __ \ / ___// /| | / // __ `// __// _ \   / /_ / / / // __ \ / ___// ___/
  / /_/ // /   / / | |/ // /_/ // /_ /  __/  / __// /_/ // / / // /__ (__  )
 / .___//_/   /_/  |___/ \__,_/ \__/ \___/  /_/   \__,_//_/ /_/ \___//____/
/_/
   ========================================================================== */


/* ==========================================================================
    Finds a node that contains 'topic' string.

    Returns pointer to found node or null.

    If 'prev' is not null, function will also return previous node that
    'next' field points to returned node. This is usefull when deleting
    nodes and saves searching for previous node to rearange list after
    delete. If function returns valid pointer, and prev is NULL, this
    means first element from the list was returned.
   ========================================================================== */


static struct psmqd_tl *psmqd_tl_find_node
(
    struct psmqd_tl  *head,  /* head of the list to search */
    const char      *topic, /* topic to look for */
    struct psmqd_tl **prev   /* previous node to returned node */
)
{
    struct psmqd_tl  *node;  /* current node */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    for (*prev = NULL, node = head; node != NULL; node = node->next)
    {
        if (strcmp(node->topic, topic) == 0)
        {
            /* this is the node you are looking for
             */
            return node;
        }

        *prev = node;
    }

    /* node of that topic does not exist
     */

    return NULL;
}


/* ==========================================================================
    Creates new node with copy of 'topic'

    Returns NULL on error or address on success

    errno:
            ENOMEM      not enough memory for new node
   ========================================================================== */


static struct psmqd_tl *psmqd_tl_new_node
(
    const char      *topic  /* topic to create new node with */
)
{
    struct psmqd_tl  *node;  /* pointer to new node */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    /* allocate enough memory for topic (plus 1 for null character)
     * and node in one malloc(), this way we will have only 1
     * allocation (for node and string) instead of 2.
     */

    node = malloc(sizeof(struct psmqd_tl) + strlen(topic) + 1);
    if (node == NULL)
    {
        return NULL;
    }

    /* point topic member to where string really is, that is right
     * after struct psmqd_tl.
     */

    node->topic = ((char *)node) + sizeof(struct psmqd_tl);

    /* make a copy of topic
     */

    strcpy(node->topic, topic);

    /* since this is new node, it doesn't point to anything
     */

    node->next = NULL;

    return node;
}


/* ==========================================================================
                       __     __ _          ____
        ____   __  __ / /_   / /(_)_____   / __/__  __ ____   _____ _____
       / __ \ / / / // __ \ / // // ___/  / /_ / / / // __ \ / ___// ___/
      / /_/ // /_/ // /_/ // // // /__   / __// /_/ // / / // /__ (__  )
     / .___/ \__,_//_.___//_//_/ \___/  /_/   \__,_//_/ /_/ \___//____/
    /_/
   ========================================================================== */


/* ==========================================================================
    Adds new node with 'topic' to list pointed by 'head'

    Function will add node just after head, not at the end of list - this is
    so we can gain some speed by not searching for last node. So when 'head'
    list is

        +---+     +---+
        | 1 | --> | 2 |
        +---+     +---+

    And node '3' is added, list will be

        +---+     +---+     +---+
        | 1 | --> | 3 | --> | 2 |
        +---+     +---+     +---+

    If 'head' is NULL (meaning list is empty), function will create new list
    and add 'topic' node to 'head'
   ========================================================================== */


int psmqd_tl_add
(
    struct psmqd_tl **head,   /* head of list where to add new node to */
    const char       *topic   /* topic for new node */
)
{
    struct psmqd_tl  *node;   /* newly created node */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    VALID(EINVAL, head);
    VALID(EINVAL, topic);

    /* create new node, let's call it 3
     *
     *           +---+
     *           | 3 |
     *           +---+
     *
     *      +---+     +---+
     *      | 1 | --> | 2 |
     *      +---+     +---+
     */

    node = psmqd_tl_new_node(topic);
    if (node == NULL)
    {
        return -1;
    }

    if (*head == NULL)
    {
        /* head is null, so list is empty and we are creating
         * new list here. In that case, simply set *head with
         * newly created node and exit
         */

        *head = node;
        return 0;
    }

    /* set new node's next field, to second item in the list,
     * if there is no second item, it will point to NULL
     *
     *           +---+
     *           | 3 |
     *           +---+
     *                 \
     *                 |
     *                 V
     *      +---+     +---+
     *      | 1 | --> | 2 |
     *      +---+     +---+
     */

    node->next = (*head)->next;

    /* set head's next to point to newly created node so our
     * list is complete once again.
     *
     *           +---+
     *           | 3 |
     *           +---+
     *           ^    \
     *           |     |
     *          /      V
     *      +---+     +---+
     *      | 1 |     | 2 |
     *      +---+     +---+
     */

    (*head)->next = node;

    return 0;
}


/* ==========================================================================
    Removes 'topic' from list 'head'.

    - if 'topic' is in 'head' node, function will modify 'head' pointer
      so 'head' points to proper node

    - if 'topic' is in 'head' node and 'head' turns out to be last element
      int the list, 'head' will become NULL
   ========================================================================== */


int psmqd_tl_delete
(
    struct psmqd_tl **head,       /* pointer to head of the list */
    const char      *topic       /* node with that topic to delete */
)
{
    struct psmqd_tl  *node;       /* found node for with 'topic' */
    struct psmqd_tl  *prev_node;  /* previous node of found 'node' */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


    VALID(EINVAL, head);
    VALID(ENOENT, *head);
    VALID(EINVAL, topic);

    node = psmqd_tl_find_node(*head, topic, &prev_node);
    if (node == NULL)
    {
        /* cannot delete node with name 'topic' because such node
         * does not exist
         */

        errno = ENOENT;
        return -1;
    }

    /* initial state of the list
     *
     *            +---+     +---+     +---+
     *   head --> | 1 | --> | 3 | --> | 2 |
     *            +---+     +---+     +---+
     */

    if (node == *head)
    {
        /* caller wants to delete node that is currently head node,
         * so we need to remove head, and them make head->next new
         * head of the list
         *
         *            +---+          +---+     +---+
         *   node --> | 1 | head --> | 3 | --> | 2 |
         *            +---+          +---+     +---+
         */

        *head = node->next;

        /* now '1' is detached from anything and can be safely freed
         */

        free(node);
        return 0;
    }

    /* node points to something else than 'head'
     *
     *            +---+     +---+     +---+     +---+
     *   head --> | 1 | --> | 3 | --> | 2 | --> | 4 |
     *            +---+     +---+     +---+     +---+
     *                                 ^
     *                                 |
     *                                node
     *
     * before deleting, we need to make sure '3' (prev node) points
     * to '4'. If node points to last element '4', then we will set
     * next member of '2' element to null.
     *
     *            +---+     +---+     +---+
     *   head --> | 1 | --> | 3 | --> | 4 |
     *            +---+     +---+     +---+
     *                                 ^
     *                                 |
     *                      +---+     /
     *             node --> | 2 | ---`
     *                      +---+
     */

    prev_node->next = node->next;

    /* now that list is consistent again, we can remove node (2)
     * without destroying list
     */

    free(node);
    return 0;
}


/* ==========================================================================
    Removes all elements in the list pointed by 'head'. After this function
    is called 'head' should no longer be used without calling psmqd_tl_new()
    on it again
   ========================================================================== */


int psmqd_tl_destroy
(
    struct psmqd_tl *head  /* list to destroy */
)
{
    struct psmqd_tl *next; /* next node to free() */
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    VALID(EINVAL, head);

    for (;head != NULL; head = next)
    {
        next = head->next;
        free(head);
    }

    return 0;
}
