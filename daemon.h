#ifndef DAEMON_H
#define DAEMON_H

#define PIPE_FILENAME ".mq.pipe"
#define LOG_FILENAME ".mq.log"

#include "logger.h"
#include "pslist.h"

typedef struct _Daemon Daemon;

struct _Daemon 
{
	char * __pipe_path;
	int __pipe;
	char * __log_path;
	Logger * __log;
	int __epfd;						/* epoll fd */
	/* TODO: should process list be volatile? */
	PsList * __pslist;				/* Process list */
	long __ncpus;					/* Number of available CPUs */
	sigset_t __blk_chld;			/* Block SIGCHLD */
};

Daemon * daemon_new (char * pipe_path, char * log_path);
void daemon_run (Daemon * self);
void daemon_setup (Daemon * self);
void daemon_stop (Daemon * self);

void daemon_run_processes (Daemon * self);

#endif /* DAEMON_H */
