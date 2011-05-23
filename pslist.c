#include <stdlib.h>
#include <stdio.h>

#include "pslist.h"

PsList * pslist_new (void)
{
	PsList * pslist = malloc (sizeof (PsList));
	if (!pslist) {
		perror ("pslist_new:malloc");
		exit (EXIT_FAILURE);
	}

	/* Initial size is CHUNK_SIZE elements: */
	pslist->list = malloc (CHUNK_SIZE * sizeof (PsList *));
	if (!pslist->list) {
		perror ("pslist_new:malloc");
		exit (EXIT_FAILURE);
	}

	pslist->len = 0;

	return pslist;
}

void pslist_append (PsList * self, Process * process)
{
	/* If the current list is full we extend it by CHUNK_SIZE: */
	if (self->len % CHUNK_SIZE == 0) {
		Process ** tmp_list;
		tmp_list = realloc (self->list, (self->len + CHUNK_SIZE) * sizeof (PsList *));
		if (tmp_list == NULL)
		{
			/* TODO: log critical but don't exit? */
			perror ("pslist_malloc:malloc");
			exit (EXIT_FAILURE);
		} else
			self->list = tmp_list;

	}

	self->list[self->len] = process;
	self->len++;
}
