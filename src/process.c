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
#include "utils.h"

#include "process.h"

#define MAX_ARGS	100		/* Maximum number of args for a command */

/* Private methods */
static char * _process_get_state_str (Process * self);
static int _process_send_signal (Process * self, int sig);


/* 
 * Create a Process object
 * args:   argv, argc
 * return: process or NULL on error
 */
Process * process_new (char ** argv)
{
	static int id = 0;	/* Initialise the unique ID */

	Process * process = malloc0 (sizeof (Process));
	if (!process)
		return NULL;

	process->_argv = argv;
	process->_state = WAITING;
	process->_pid = 0;
	process->uid = id;
	process->_ret = 0;
	process->to_remove = 0;

	/* Increment the id */
	id++;

	return process;
}

/*
 * Delete and free a Process
 * args:   Message
 * return: void
 */
void process_del (Process * self)
{
	int i;

	/* Free each arg in argv */
	for (i = 0; self->_argv[i] != NULL; i++)
		free (self->_argv[i]);

	free (self->_argv);
	free (self);
}

/* 
 * Generate a string representation of the process
 * args:   self
 * return: string or NULL on error
 */
char * process_str (Process * self)
{
	char command[STR_MAX_LEN - STR_MAX_UID_LEN -
				 STR_MAX_STATE_LEN - STR_MAX_EXIT_LEN];
	char * ret, * current, * state;
	size_t len = 0, total_len = 0;
	int i;
	short int cut = 0;			/* Were the args cut to fit in command string? */

	/* Initialise position of current argv in 'command' */
	current = command;

	/* Transform argv into a string of up to STR_MAX_LEN - STR_MAX_UID_LEN */
	for (i = 0; self->_argv[i] != NULL; i++)
	{
		/* Get arg length */
		len = strlen (self->_argv[i]);
		if (len < 1)
			continue;

		/* Check how much space left we have */
		if (total_len + len + 1 > STR_MAX_LEN - STR_MAX_UID_LEN -
			STR_MAX_STATE_LEN - STR_MAX_EXIT_LEN)
		{
			len = STR_MAX_LEN - STR_MAX_UID_LEN -
				  STR_MAX_STATE_LEN - STR_MAX_EXIT_LEN - total_len;
			cut = 1;
		}

		/* Copy arg in 'command' at current position */
		strncpy (current, self->_argv[i], len);

		/* Add a separation whitespace */
		*(current + len) = ' ';

		total_len += len + 1;
		current += len + 1;

		if (cut)
			break;
	}

	if (cut)
		strncpy (command + (total_len - 7), " (...)\0", 7);
	else
		command[total_len - 1] = '\0';	/* -1 removes last separation whitespace */	

	/* Get Process state's string */
	state = _process_get_state_str (self);

	/* If the process exited we print the exit code */
	if (self->_state == EXITED || self->_state == KILLED)
		ret = msprintf ("%-4d %-3s %-4d %s ", self->uid, state,
						self->_ret, command);	/* "4d": STR_MAX_UID_LEN - 1 */
	else
		ret = msprintf ("%-4d %-8s %s ", self->uid,
						state, command);	/* "4d": STR_MAX_UID_LEN - 1 */

	return ret;
}

/* 
 * Execute the command: 
 * args:   Process
 * return: 0 on success
 */
int process_run (Process * self)
{
	sigset_t set;

	/* Create new process */
	self->_pid = fork ();
	if (self->_pid == -1)
		return 1;	/* failed */
	else if (self->_pid != 0) {
		self->_state = RUNNING;
		return 0;	/* success */
	}

	/* Unblock SIGTERM and SIGHUP (this was set in daemon 
	 * and is kept after the fork) */

	/* Create a new empty set */
	if (sigemptyset (&set) == -1)
		exit (EXIT_FAILURE);

	/* Replace the current mask */
	if (sigprocmask (SIG_SETMASK, &set, NULL) == -1)
		exit (EXIT_FAILURE);

	/* Exec the process */
	execvp (self->_argv[0], self->_argv);

	/* FIXME: find a way to report exec errors to the main process */

	/* if we reach this something went wrong*/
	exit (EXIT_FAILURE);
}

/* 
 * "Wait" on the process
 * args:   Process, siginfo_t from signal
 * return: 0 on success
 */
int process_wait (Process * self, siginfo_t * siginfo)
{
	/* Check signal code */
	switch (siginfo->si_code)
	{
		case CLD_EXITED:
			self->_state = EXITED;
			self->_ret = siginfo->si_status;
			break;
		case CLD_KILLED:
			self->_state = KILLED;
			self->_ret = siginfo->si_status;
			break;
		case CLD_DUMPED:
			self->_state = DUMPED;
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
 * Send the given signal to the process
 * args:   Process
 * return: 0 on success, 1 on error
 */
int process_kill (Process * self, int sig)
{
	/* Kill only makes sense if the process is currently running */
	if (self->_state != RUNNING)
		return 0;

	return _process_send_signal (self, sig);
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
	return self->_state;
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
	return self->_pid;
}


/* Private methods */

/* 
 * Return the process's state string
 * args:   Process
 * return: state's string
 */
static char * _process_get_state_str (Process * self)
{
	/* Get string for the Process' state */
	switch (self->_state)
	{
		case WAITING:
			return "W";
		case RUNNING:
			return "R*";
		case EXITED:
			return "C";		/* COMPLETE */
		case KILLED:
			return "K";
		case DUMPED:
			return "D";
		case STOPPED:
			return "S";
		default:
			return "?";
	}
}

/* 
 * Send signal to process
 * args:   Process, signal
 * return: 0 on success, 1 on error
 */
static int _process_send_signal (Process * self, int sig)
{

	/* Sent signal to the process */
	if (kill (self->_pid, sig) == -1)
		return 1;

	return 0;
}
