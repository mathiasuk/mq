/* 
 * This file is part of mq.
 * mq - src/daemon.h
 * Copyright (C) 2011 Mathias Andre <mathias@acronycal.org>
 *
 * mq is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mq.  If not, see <http://www.gnu.org/licenses/>.
 */

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
