/* 
 * This file is part of mq.
 * mq - src/process.h
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

#ifndef PROCESS_H
#define PROCESS_H

#include <unistd.h>
#include <signal.h>

typedef enum {
	ANY,		/* Matches any other state, used to get list of processes */
	WAITING,	/* Waiting to be started */
	RUNNING,	/* Currently running */
	EXITED,		/* Exited normally */
	KILLED,		/* Killed */
	DUMPED,		/* Terminated abnormally */
	STOPPED,	/* Stopped */
} PsState;

typedef struct _Process Process;

struct _Process 
{
	const char * __command;
	PsState __state;
	pid_t __pid;
	int __ret;		/* Return value */
};

Process * process_new (const char * command);
char * process_str (Process * self);
int process_run (Process * self);
int process_wait (Process * self, siginfo_t * siginfo);

PsState process_get_state (Process * self);
pid_t process_get_pid (Process * self);

#endif /* PROCESS_H */