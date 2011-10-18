/* 
 * This file is part of mq.
 * mq - src/messagelist.c
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

#include "messagelist.h"
#include "utils.h"

/* 
 * Wrapper around list_new ()
 * args:   void
 * return: MessageList or NULL on error
 */
MessageList * messagelist_new (void)
{
	return list_new ();
}

/* Delete and free the list
 * args: MessageList
 * return: void
 */
void messagelist_delete (MessageList * self)
{
	list_delete (self);
}

/* 
 * Wrapper around list_append ()
 * args:   MessageList, Message
 * return: 0 on success
 */
int messagelist_append (MessageList * self, Message * message)
{
	return list_append (self, message);
}

/*
 * Wrapper around list_remove ()
 * args:   List, pointer to item
 * return: 0 on sucess, 1 if item not found, -1 on error
 */
int messagelist_remove (MessageList * self, Message * message)
{
	return list_remove (self, message);
}

/* 
 * Wrapper around list_get_item ()
 * args:   MessageList, index
 * return: Message at index, or NULL on error
 */
Message * messagelist_get_message (MessageList * self, int index)
{
	return list_get_item (self, index);
}

/* 
 * Get Messages for sock (if any)
 * args:   MessageList, sock
 * return: Message with sock, or NULL
 */
Message * messagelist_get_message_by_sock (MessageList * self, int sock)
{
	int i;
	Message * m;

	for (i = 0; i < self->_len; i++) {
		m = messagelist_get_message (self, i);
		if (m->sock == sock)
			return m;
	}

	/* sock not found */
	return NULL;
}
