#include <stdlib.h>

#include "daemon.h"

int main (void)
{
	Daemon * d = daemon_new();
	daemon_run(d);	

	return EXIT_SUCCESS;
}
