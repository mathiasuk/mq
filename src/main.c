#include <stdlib.h>

#include "daemon.h"

Daemon * d;

int main (void)
{
	d = daemon_new(NULL, NULL);
	if (d == NULL) {
		perror ("main:daemon_new");
		exit (EXIT_FAILURE);
	}
	daemon_setup(d);
	daemon_run(d);	

	return EXIT_SUCCESS;
}
