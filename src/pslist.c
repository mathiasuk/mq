/* 
 * This file is part of mq.
 * mq - src/pslist.c
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

	/* Initial size is CHUNK_SIZE elements */
	pslist->__list = malloc (CHUNK_SIZE * sizeof (PsList *));
	if (!pslist->__list)
		return NULL;

	pslist->__len = 0;

	return pslist;
}

/* 
 * Add a Process to the PsList
 * args:   PsList, Process
 * return: 0 on success
 */
int pslist_append (PsList * self, Process * process)
{
	/* If the current list is full we extend it by CHUNK_SIZE */
	if (self->__len % CHUNK_SIZE == 0) {
		Process ** tmp_list;
		tmp_list = realloc (self->__list, (self->__len + CHUNK_SIZE) * sizeof (PsList *));
		if (tmp_list == NULL)
			return -1;
		else
			self->__list = tmp_list;

	}

	self->__list[self->__len] = process;
	self->__len++;
	return 0;
}

/*
 * Get the list of processes in a given state
 * args:   Pslist, state, pointer to a list of indexes
 * return: number of matching processes or -1 on error
 *         the pointer to the list of indexes should be freed after usage 
 */
int pslist_get_nps (PsList * self, PsState state, int * list)
{
	int i, len = 0;
	Process * p = NULL;

	if (self->__len == 0)
		return 0;

	for (i = 0; i < self->__len; i++) {
		p = pslist_get_ps (self, i);
		if ((state == ANY) || (process_get_state (p) == state)) {
			if (list)
				list[len] = i;
			len++;
		}
	}

	return len;
}

/* 
 * Get the process at the given index
 * args:   Pslit, index
 * return: Process at index, or NULL on error
 */
Process * pslist_get_ps (PsList * self, int index)
{
	/* Check that the given index is within the range */
	if ((index < 0) || (index >= self->__len))
		return NULL;

	return self->__list[index];
}
