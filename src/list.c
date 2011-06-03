/* 
 * This file is part of mq.
 * mq - src/list.c
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

#include "list.h"

/* 
 * Create and initialise the List
 * args:   void
 * return: List or NULL on error
 */
List * list_new (void)
{
	List * list = malloc (sizeof (List));
	if (list == NULL)
		return NULL;

	/* Initial size is CHUNK_SIZE elements */
	list->_list = malloc (CHUNK_SIZE * sizeof (List *));
	if (!list->_list)
		return NULL;

	list->_len = 0;

	return list;
}

/* 
 * Add an item to the List
 * args:   List, item
 * return: 0 on success, 1 on error
 */
int list_append (List * self, void * item)
{
	/* If the current list is full we extend it by CHUNK_SIZE */
	if (self->_len % CHUNK_SIZE == 0) {
		void ** tmp_list;
		tmp_list = realloc (self->_list, (self->_len + CHUNK_SIZE) * sizeof (List *));
		if (tmp_list == NULL)
			return 1;
		else
			self->_list = tmp_list;

	}

	self->_list[self->_len] = item;
	self->_len++;
	return 0;
}

/* 
 * Get the item at the given index
 * args:   List, index
 * return: item at index, or NULL on error
 */
void * list_get_item (List * self, int index)
{
	/* Check that the given index is within the range */
	if ((index < 0) || (index >= self->_len))
		return NULL;

	return self->_list[index];
}