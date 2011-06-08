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
#include "utils.h"

/* 
 * Wrapper around list_new ()
 * args:   void
 * return: PsList or NULL on error
 */
PsList * pslist_new (void)
{
	return list_new ();
}

/* 
 * Wrapper around list_append ()
 * args:   PsList, Process
 * return: 0 on success
 */
int pslist_append (PsList * self, Process * process)
{
	return list_append (self, process);
}

/* 
 * Wrapper around list_get_item ()
 * args:   Pslist, index
 * return: Process at index, or NULL on error
 */
Process * pslist_get_ps (PsList * self, int index)
{
	return list_get_item (self, index);
}

/*
 * Wrapper around list_move_items ()
 * args:   PsList, start position, number of items to move, 
 *		   destination position (or -1 to move to end of list)
 * return: 0 on success, 1 on error
 */
int pslist_move_items (List * self, int start, int count, int dest)
{
	return list_get_item (self, start, count, dest);
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

	if (self->_len == 0)
		return 0;

	for (i = 0; i < self->_len; i++) {
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
 * Get the process with given PID
 * args:   Pslit, PID
 * return: Process with PID, or NULL on error
 */
Process * pslist_get_ps_by_pid (PsList * self, pid_t pid)
{
	int i;
	Process * p;

	if (pid < 1)
		return NULL;

	for (i = 0; i < self->_len; i++) {
		p = pslist_get_ps (self, i);
		if (process_get_pid (p) == pid)
			return p;
	}

	/* pid not found */
	return NULL;
}
