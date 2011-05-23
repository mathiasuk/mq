#ifndef DAEMON_H
#define DAEMON_H

#define PIPE_PATH ".mq.pipe"

#include "logger.h"

typedef struct _Daemon Daemon;

struct _Daemon 
{
	int pipe;
	char * pipe_path;
	Logger * log;
	int epfd;		                /* epoll fd */
};

Daemon * daemon_new (char * pipe_path);
void daemon_run (Daemon * self);
void daemon_setup (Daemon * self);

#endif /* DAEMON_H */
