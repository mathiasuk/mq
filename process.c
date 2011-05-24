#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "process.h"

#define MAX_ARGS	100		/* Maximum number of args for a command */

Process * process_new (const char * command)
{
	Process * process = malloc (sizeof (Process));
	if (!process) {
		perror ("process_new:malloc");
		exit (EXIT_FAILURE);
	}

	process->command = command;
	process->state = WAITING;
	process->pid = 0;

	return process;
}

/* Returns a string representation of the process: */
char * process_print (Process * self)
{
	char * ret;
	if ((ret = malloc (snprintf (NULL, 0, "%-30s", self->command))) == NULL) {
		perror ("process_print:malloc");
		exit (EXIT_FAILURE);
	}
	sprintf (ret, "%-30s", self->command);

	return ret;
}

/* Execute the command: 
 * Return: 0 on success */

int process_run (Process * self)
{
	char * args[MAX_ARGS];
	char * command = strdup (self->command);
	int i = 0;

	/* Transform the command line in an array
	 * of null terminated strings: */
	args[i++] = command;
	while (*command != '\0') {
		if (*command == ' ') {
			*command = '\0';
			args[i++] = ++command;
		} else ++command;
	}
	args[i] = NULL;

	/* Create new process */
	self->pid = fork ();
	if (self->pid == -1)
		return 1;	/* failed */
	else if (self->pid != 0)
		return 0;	/* success */

	/* Exec the process: */
	execvp (args[0], args);

	return 1;	/* if we reach this something went wrong*/
}
