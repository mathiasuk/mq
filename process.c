#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
	if ((ret = malloc (snprintf (NULL, 0, "%-30s, PID: %d", self->__command, self->__pid))) == NULL)
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
