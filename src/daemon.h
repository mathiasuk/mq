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

#include <signal.h>
#include <sys/un.h>

#include "logger.h"
#include "pslist.h"
#include "messagelist.h"

typedef struct _Daemon Daemon;

struct _Daemon 
{
	char * _sock_path;
	int _sock;
	struct sockaddr_un _slocal;
	char * _pid_path;
	Logger * _log;
	char * _log_path;
	short int _running;		/* whether the daemon is running or not */
	int _epfd;				/* epoll fd */
	PsList * _pslist;		/* Process list, these should only be accessed while
							   signals are blocked with _daemon_block_signals */
	MessageList * _mlist;	/* List of messages to be sent to sockets */
	long _ncpus;			/* Number of available CPUs */
	sigset_t _sig_mask;		/* Mask to block signals*/
};

Daemon * daemon_new (char * sock_path, char * pid_path, char * log_path);
void daemon_delete (Daemon * self);
void daemon_run (Daemon * self);

#endif /* DAEMON_H */
