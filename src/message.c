/* 
 * This file is part of mq.
 * mq - src/message.c
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
#include <sys/socket.h>

#include "message.h"

/* 
 * Create and initialise a message
 * args:   message type, message string
 * return: Message object or NULL on error
 */
Message * message_new (MessageType type, char * content, int sock)
{
	Message * message = malloc (sizeof (Message));

	if (message == NULL)
		return NULL;

	message->_type = type;
	message->_content = content;
	message->sock = sock;

	return message;
}

/*
 * Send the current message on the socket
 * args:   Message
 * return: 0 on success, 1 on eror
 */
int message_send (Message * self)
{
	/* Check that the socket is valid */
	if (self->sock < 1)
		return 1;

	if (send (self->sock, &self->_type, sizeof (MessageType), 0) == -1)
		return 1;
	return 0;
}
