#include <stdlib.h>
#include <stdio.h>

#include "pslist.h"

/* 
 * Create and initialise the PsList
 * args:   void
 * return: PsList or NULL on error
 */
PsList * pslist_new (void)
{
	PsList * pslist = malloc (sizeof (PsList));
	if (!pslist)
		return NULL;

	/* Initial size is CHUNK_SIZE elements: */
	pslist->list = malloc (CHUNK_SIZE * sizeof (PsList *));
	if (!pslist->list)
		return NULL;

	pslist->len = 0;

	return pslist;
}

/* 
 * Add a Process to the PsList
 * args:   PsList, Process
 * return: 0 on success
 */
int pslist_append (PsList * self, Process * process)
{
	/* If the current list is full we extend it by CHUNK_SIZE: */
	if (self->len % CHUNK_SIZE == 0) {
		Process ** tmp_list;
		tmp_list = realloc (self->list, (self->len + CHUNK_SIZE) * sizeof (PsList *));
		if (tmp_list == NULL)
			return -1;
		else
			self->list = tmp_list;

	}

	self->list[self->len] = process;
	self->len++;
	return 0;
}
