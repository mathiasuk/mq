#ifndef DAEMON_H
#define DAEMON_H

#define PIPE_PATH "/var/run/mq.pipe"

#include "logger.h"

typedef struct _Daemon Daemon;

struct _Daemon 
{
	int pipe;
	char * pipe_path;
	Logger * log;
};

Daemon * daemon_new (void);
void daemon_run (Daemon * self);

#endif /* DAEMON_H */
