#ifndef DAEMON_H
#define DAEMON_H

#define PIPE_FILENAME ".mq.pipe"
#define LOG_FILENAME ".mq.log"

#include "logger.h"

typedef struct _Daemon Daemon;

struct _Daemon 
{
	int pipe;
	char * pipe_path;
	char * log_path;
	Logger * log;
	int epfd;		                /* epoll fd */
};

Daemon * daemon_new (char * pipe_path, char * log_path);
void daemon_run (Daemon * self);
void daemon_setup (Daemon * self);

#endif /* DAEMON_H */
