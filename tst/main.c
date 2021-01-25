/* ==========================================================================
    Licensed under BSD 2clause license See LICENSE file for more information
    Author: Michał Łyszczek <michal.lyszczek@bofc.pl>
   ========================================================================== */

#include "mtest.h"

#include <embedlog.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

mt_defs();  /* definitions for mtest */

void psmqd_cfg_test_group(void);
void psmqd_tl_test_group(void);
void psmqd_test_group(void);
void psmq_test_group(void);

int main(void)
{
#if TEST_ENABLE_RANDOM_SEED
	int fd;
	unsigned int seed;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	seed = time(NULL);

	fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0)
	{
		int shut_up_gcc = read(fd, &seed, sizeof(seed));
		(void)shut_up_gcc;
		close(fd);
	}

	srand(seed);
	fprintf(stderr, "seed value: %u\n", seed);
#else
	srand(1);
	fprintf(stderr, "seed value: 1\n");
#endif

	el_init();
	el_option(EL_OUT, EL_OUT_STDERR);
	el_option(EL_COLORS, 1);
	el_option(EL_FINFO, 1);
	psmqd_cfg_test_group();
	psmqd_tl_test_group();
	psmqd_test_group();
	psmq_test_group();
	el_cleanup();
	mt_return();
}
