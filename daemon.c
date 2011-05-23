#include <stdlib.h>

#include "daemon.h"
#include "logger.h"

Daemon * daemon_new (void)
{
	Daemon * daemon = malloc (sizeof (Daemon));
	if (!daemon) {
		perror ("daemon_new:malloc");
		exit (EXIT_FAILURE);
	}

	daemon->pipe = -1;
	daemon->pipe_path = PIPE_PATH;
//	daemon->log = logger_new (NULL);
	daemon->log = logger_new ("/tmp/mq.log");

	return daemon;
}

void daemon_run (Daemon * self)
{
	logger_info(self->log, "This is a test");
	return ;
}
