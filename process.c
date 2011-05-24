#include <stdlib.h>
#include <stdio.h>

#include "process.h"

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
