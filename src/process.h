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

#define STR_MAX_LEN 80		/* Max string length for displaying process info */
#define STR_MAX_UID_LEN 5	/* Max string length for uid + whitespace */
#define STR_MAX_STATE_LEN 5	/* Max string length for state + whitespace */
#define STR_MAX_EXIT_LEN 5	/* Max string length for exit status + whitespace */

#include <unistd.h>
#include <signal.h>

typedef enum {
	/* FIXME: is ANY necessary? */
	ANY,		/* Matches any other state, used to get list of processes */
	WAITING,	/* Waiting to be started */
	RUNNING,	/* Currently running */
	EXITED,		/* Exited normally */
	KILLED,		/* Killed */
	DUMPED,		/* Terminated abnormally */
	STOPPED		/* Stopped */
} PsState;

typedef struct _Process Process;

struct _Process 
{
	char ** _argv;	/* Arguments for the process, NULL terminated */
	PsState _state;
	pid_t _pid;
	int uid;		/* Unique process ID */
	int _ret;		/* Return value */
	short int to_remove;	/* Indicate that the process should be 
							   removed once done running */
};

Process * process_new (char ** argv);
void process_del (Process * self);
char * process_str (Process * self);
int process_run (Process * self);
int process_wait (Process * self, siginfo_t * siginfo);
int process_kill (Process * self, int sig);

PsState process_get_state (Process * self);
pid_t process_get_pid (Process * self);

#endif /* PROCESS_H */
