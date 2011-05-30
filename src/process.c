/* 
 * This file is part of mq.
 * mq - src/process.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "process.h"

#define MAX_ARGS	100		/* Maximum number of args for a command */

/* 
 * Create a Process object
 * args:   command line
 * return: process or NULL on error
 */
Process * process_new (const char * command)
{
	Process * process = malloc (sizeof (Process));
	if (!process)
		return NULL;

	process->__command = command;
	process->__state = WAITING;
	process->__pid = 0;
	process->__ret = 0;

	return process;
}

/* 
 * Generate a string representation of the process
 * args:   self
 * return: string or NULL on error
 */
char * process_str (Process * self)
{
	char * ret;
	/* TODO: only show PID if process is running? */
	if ((ret = malloc (snprintf (NULL, 0, "%-30s, PID: %d", self->__command,
								 self->__pid))) == NULL)
		return NULL;
	sprintf (ret, "%-30s, PID: %d", self->__command, self->__pid);

	return ret;
}

/* 
 * Execute the command: 
 * args:   Process
 * return: 0 on success
 */
int process_run (Process * self)
{
	char * args[MAX_ARGS];
	char * command = strdup (self->__command);
	int i = 0;

	/* Transform the command line in an array
	 * of null terminated strings */
	args[i++] = command;
	while (*command != '\0') {
		if (*command == ' ') {
			*command = '\0';
			args[i++] = ++command;
		} else ++command;
	}
	args[i] = NULL;

	/* Create new process */
	self->__pid = fork ();
	if (self->__pid == -1)
		return 1;	/* failed */
	else if (self->__pid != 0) {
		self->__state = RUNNING;
		return 0;	/* success */
	}

	/* Exec the process */
	execvp (args[0], args);

	return 1;	/* if we reach this something went wrong*/
}

/* 
 * "Wait" on the process
 * args:   Process, siginfo_t from signal
 * return: 0 on sucess
 */
int process_wait (Process * self, siginfo_t * siginfo)
{
	/* Check signal code */
	switch (siginfo->si_code)
	{
		case CLD_EXITED:
			self->__state = EXITED;
			self->__ret = siginfo->si_status;
			break;
		case CLD_KILLED:
			self->__state = KILLED;
			break;
		case CLD_DUMPED:
			self->__state = DUMPED;
			break;
		case CLD_STOPPED:
			/* TODO */
			break;
		case CLD_CONTINUED:
			/* TODO */
			break;
		default:
			/* This should not happen */
			return 1;

	}


	return 0;
}

/*
 * Return the process' state
 * args:   Process
 * return: PsState or -1 on error
 */
PsState process_get_state (Process * self)
{
	if (self == NULL)
		return -1;
	return self->__state;
}

/*
 * Return the process' pid
 * args:   Process
 * return: pid or -1 on error
 */
pid_t process_get_pid (Process * self)
{
	if (self == NULL)
		return -1;
	return self->__pid;
}
