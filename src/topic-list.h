/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#ifndef PSMQ_TOPIC_LIST_H
#define PSMQ_TOPIC_LIST_H 1

struct psmqd_tl
{
    char            *topic;
    struct psmqd_tl  *next;
};

int psmqd_tl_add(struct psmqd_tl **head, const char *topic);
int psmqd_tl_delete(struct psmqd_tl **head, const char *topic);
int psmqd_tl_destroy(struct psmqd_tl *head);

#endif /* PSMQ_TOPIC_LIST_H */
