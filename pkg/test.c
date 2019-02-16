/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

/* this is very minimum program, just to check if psmq is installed
 * on the system or not
 */

#include <psmq.h>
#include <stdio.h>

int main(void)
{
    /* if it compiles - it works
     */

    psmq_init(NULL, NULL, NULL, 0);
    fprintf(stderr, "psmq works!\n");
    return 0;
}
