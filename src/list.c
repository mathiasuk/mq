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
#include "utils.h"

/* 
 * Create and initialise the List
 * args:   void
 * return: List or NULL on error
 */
List * list_new (void)
{
	List * list = malloc0 (sizeof (List));
	if (list == NULL)
		return NULL;

	/* Initial size is CHUNK_SIZE elements */
	list->_list = malloc0 (CHUNK_SIZE * sizeof (List *));
	if (!list->_list)
		return NULL;

	list->_len = 0;

	return list;
}

/*
 * Returns lenght of list
 * args:   List
 * return: number of items in List
 */
int list_len (List * self)
{
	return self->_len;
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
 * Remove item from the list
 * args:   List, pointer to item
 * return: 0 on sucess, 1 if item not found, -1 on error
 */
int list_remove (List * self, void * item)
{
	int i, j;

	/* Look for the item in list */
	for (i = 0; i < self->_len && self->_list[i] != item; i++)
		;

	if (i == self->_len)
		/* Couldn't find item */
		return 1;

	/* Left shift all items after our item, effectively removing it */
	for (j = i; j < self->_len - 2; j ++)
		self->_list[j] = self->_list[j + 1];

	/* If we have more than CHUNK_SIZE free elements we compress the list*/
	if (self->_len % CHUNK_SIZE == 0) {
		void ** tmp_list;
		tmp_list = realloc (self->_list, (self->_len) * sizeof (List *));
		if (tmp_list == NULL)
			return 1;
		else
			self->_list = tmp_list;

	}

	/* Decrease the lengh of the list */
	self->_len--;

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

/*
 * Move count items from origin to destination
 * args:   List, start position, number of items to move, 
 *		   destination position (or -1 to move to end of list)
 * return: 0 on success, 1 on error
 */
int list_move_items (List * self, int start, int count, int dest)
{
	int i;
	void ** p = malloc0 (count * sizeof (void *));

	if (p == NULL)
		return 1;

	/* Check that the starting position is within the list range */
	if (start < 0 || start >= self->_len)
		return 1;

	/* Check that the destination position is within the list range */
	if (dest < 0 || dest >= self->_len)
		return 1;

	/* Check that elements from start until start + count are in the range */
	if (start + count - 1 >= self->_len)
		return 1;

	if (start == dest)
		/* Nothing to do */
		return 0;

	/* Save the values to move */
	for (i = 0; i < count; i++)
	{
		p[i] = self->_list[start + i];
	}

	for (i = 0; i < dest - start; i++)
	{
		self->_list[start + i] = self->_list[start + count + i];
	}
	
	/* Restore the values in their new positions */
	for (i = count - 1; i >= 0; i--)
	{
		self->_list[start + dest - start + i] = p[i];
	}

	free (p);

	return 0;
}
