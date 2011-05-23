#include <stdlib.h>

#include "daemon.h"

int main (void)
{
	Daemon * d = daemon_new(NULL, NULL);
	daemon_setup(d);
	daemon_run(d);	

	return EXIT_SUCCESS;
}
